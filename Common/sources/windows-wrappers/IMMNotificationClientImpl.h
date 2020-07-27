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
#include <functional>
#include <mutex>

namespace rxtd::utils {
	class CMMNotificationClient : public IUnknownImpl<IMMNotificationClient> {
	public:
		using EventCallback = std::function<void(sview deviceId)>;

	private:
		EventCallback eventCallback;
		IMMDeviceEnumeratorWrapper enumerator;

		std::mutex mut;

	public:
		CMMNotificationClient() = default;

		virtual ~CMMNotificationClient() {
			std::lock_guard<std::mutex> lock{ mut };
			enumerator.getPointer()->UnregisterEndpointNotificationCallback(this);
		}

		CMMNotificationClient(EventCallback eventCallback, IMMDeviceEnumeratorWrapper _enumerator) :
			eventCallback(std::move(eventCallback)), enumerator(std::move(_enumerator)) {
			enumerator.getPointer()->RegisterEndpointNotificationCallback(this);
		}

		CMMNotificationClient(const CMMNotificationClient& other) = delete;
		CMMNotificationClient(CMMNotificationClient&& other) noexcept = delete;
		CMMNotificationClient& operator=(const CMMNotificationClient& other) = delete;
		CMMNotificationClient& operator=(CMMNotificationClient&& other) noexcept = delete;

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
			std::lock_guard<std::mutex> lock { mut };
			callback(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceAdded(const wchar_t* deviceId) override {
			std::lock_guard<std::mutex> lock { mut };
			callback(deviceId);
			return S_OK;
		};

		HRESULT STDMETHODCALLTYPE OnDeviceRemoved(const wchar_t* deviceId) override {
			std::lock_guard<std::mutex> lock { mut };
			callback(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(const wchar_t* deviceId, DWORD dwNewState) override {
			std::lock_guard<std::mutex> lock { mut };
			callback(deviceId);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(const wchar_t* deviceId, const PROPERTYKEY key) override {
			std::lock_guard<std::mutex> lock { mut };
			callback(deviceId);
			return S_OK;
		}

	private:
		void callback(sview deviceId) {
			if (eventCallback != nullptr) {
				eventCallback(deviceId);
			}
		}
	};
}
