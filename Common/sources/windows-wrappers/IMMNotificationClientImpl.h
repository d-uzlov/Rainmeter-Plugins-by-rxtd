/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "IUnknownImpl.h"
#include "IMMDeviceEnumeratorWrapper.h"
#include <mmdeviceapi.h>
#include <mutex>
#include <set>

// You've instantiated std::atomic<T> with sizeof(T) equal to 2/4/8 and alignof(T) < sizeof(T).
// Before VS 2015 Update 2, this would have misbehaved at runtime.
// VS 2015 Update 2 was fixed to handle this correctly, but the fix inherently changes layout and breaks binary compatibility.
// Please define _ENABLE_ATOMIC_ALIGNMENT_FIX to acknowledge that you understand this,
// and that everything you're linking has been compiled with VS 2015 Update 2 (or later).
// #define _ENABLE_ATOMIC_ALIGNMENT_FIX
#include <atomic>

namespace rxtd::utils {
	class CMMNotificationClient : public IUnknownImpl<IMMNotificationClient> {
	public:
		struct Changes {
			bool defaultRender;
			bool defaultCapture;
			std::set<string, std::less<>> devices;
		};

	private:
		IMMDeviceEnumeratorWrapper enumerator;
		bool registered = false;

		std::atomic<bool> defaultRenderChanged{ };
		std::atomic<bool> defaultCaptureChanged{ };
		std::set<string, std::less<>> devicesWithChangedState;

		std::mutex mut;

		// when Rainmeter::Logger is used in functions here,
		// somewhere in the ntdll.dll deadlock can occur

	public:
		CMMNotificationClient() = default;

		virtual ~CMMNotificationClient() {
			unregister();
		}

		CMMNotificationClient(IMMDeviceEnumeratorWrapper _enumerator) :
			enumerator(std::move(_enumerator)) {
			const auto result = enumerator.getPointer()->RegisterEndpointNotificationCallback(this);
			if (result == S_OK) {
				registered = true;
			}
		}

		CMMNotificationClient(const CMMNotificationClient& other) = delete;
		CMMNotificationClient(CMMNotificationClient&& other) noexcept = delete;
		CMMNotificationClient& operator=(const CMMNotificationClient& other) = delete;
		CMMNotificationClient& operator=(CMMNotificationClient&& other) noexcept = delete;

		void unregister() {
			if (registered == false) {
				return;
			}

			enumerator.getPointer()->UnregisterEndpointNotificationCallback(this);
			registered = false;
		}

		[[nodiscard]]
		Changes takeChanges() {
			std::lock_guard<std::mutex> lock{ mut };
			Changes result{ };
			result.defaultRender = defaultRenderChanged.exchange(false);
			result.defaultCapture = defaultCaptureChanged.exchange(false);
			result.devices = std::move(devicesWithChangedState);
			return result;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvInterface) override {
			const auto result = IUnknownImpl::QueryInterface(riid, ppvInterface);
			if (result == S_OK) {
				return result;
			}

			if (__uuidof(IMMNotificationClient) == riid) {
				AddRef();
				*ppvInterface = this;
			} else {
				*ppvInterface = nullptr;
				return E_NOINTERFACE;
			}
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, const wchar_t* deviceId) override {
			// this function can have (deviceId == nullptr) when no default device is available
			// all other have proper non-null string

			if (role == eConsole) {
				if (flow == eCapture) {
					defaultCaptureChanged.exchange(true);
				} else {
					defaultRenderChanged.exchange(true);
				}
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceAdded(const wchar_t* deviceId) override {
			std::lock_guard<std::mutex> lock{ mut };
			devicesWithChangedState.insert(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceRemoved(const wchar_t* deviceId) override {
			std::lock_guard<std::mutex> lock{ mut };
			devicesWithChangedState.insert(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(const wchar_t* deviceId, DWORD dwNewState) override {
			std::lock_guard<std::mutex> lock{ mut };
			devicesWithChangedState.insert(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(const wchar_t* deviceId, const PROPERTYKEY key) override {
			std::lock_guard<std::mutex> lock{ mut };
			devicesWithChangedState.insert(deviceId);
			return S_OK;
		}
	};
}
