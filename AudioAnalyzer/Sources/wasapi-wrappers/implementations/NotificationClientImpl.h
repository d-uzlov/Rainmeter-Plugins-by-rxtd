/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <functional>
#include <mutex>
#include <set>

#include <mmdeviceapi.h>

#include "../IMMDeviceEnumeratorWrapper.h"
#include "winapi-wrappers/implementations/IUnknownImpl.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	class MediaDeviceListNotificationClient : public common::winapi_wrappers::IUnknownImpl<IMMNotificationClient> {
	public:
		enum class DefaultDeviceChange {
			eNONE,
			eCHANGED,
			eNO_DEVICE
		};

		struct Changes {
			DefaultDeviceChange defaultOutputChange = DefaultDeviceChange::eNONE;
			DefaultDeviceChange defaultInputChange = DefaultDeviceChange::eNONE;
			std::set<string, std::less<>> stateChanged;
			std::set<string, std::less<>> removed;
		};

	private:
		IMMDeviceEnumeratorWrapper enumerator;
		std::mutex mut;

		std::function<void()> changesCallback;

		Changes changes;
		std::set<string, std::less<>> activeDevices;
		string defaultInputId;
		string defaultOutputId;

	public:
		static MediaDeviceListNotificationClient* create() {
			return new MediaDeviceListNotificationClient;
		}

	private:
		MediaDeviceListNotificationClient() = default;

	protected:
		~MediaDeviceListNotificationClient() = default;

	public:
		void init(IMMDeviceEnumeratorWrapper& enumerator) {
			auto lock = getLock();
			enumerator.ref().RegisterEndpointNotificationCallback(this);

			for (auto& dev : enumerator.getActiveDevices(MediaDeviceType::eINPUT)) {
				activeDevices.insert(dev.getId() % own());
			}

			for (auto& dev : enumerator.getActiveDevices(MediaDeviceType::eOUTPUT)) {
				activeDevices.insert(dev.getId() % own());
			}

			defaultInputId = enumerator.getDefaultDevice(MediaDeviceType::eINPUT).value_or(MediaDeviceWrapper{}).getId();
			defaultInputId = enumerator.getDefaultDevice(MediaDeviceType::eOUTPUT).value_or(MediaDeviceWrapper{}).getId();
		}

		void deinit(IMMDeviceEnumeratorWrapper& enumerator) {
			enumerator.ref().UnregisterEndpointNotificationCallback(this);
		}

		// callback should not call any methods of the object
		// any such call will result in a deadlock
		void setCallback(std::function<void()> value) {
			auto lock = getLock();
			changesCallback = std::move(value);
		}

		[[nodiscard]]
		Changes takeChanges() {
			auto lock = getLock();
			return std::exchange(changes, {});
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
			if (role != eConsole) {
				return S_OK;
			}

			auto lock = getLock();

			// if (deviceId == nullptr) then no default device is available
			// this can only happen in #OnDefaultDeviceChanged
			const auto change = deviceId == nullptr ? DefaultDeviceChange::eNO_DEVICE : DefaultDeviceChange::eCHANGED;

			if (deviceId == nullptr) {
				deviceId = L"";
			}

			if (flow == eCapture) {
				if (defaultInputId != deviceId) {
					defaultInputId = deviceId;
					changes.defaultInputChange = change;
				}
			} else {
				if (defaultOutputId != deviceId) {
					defaultOutputId = deviceId;
					changes.defaultOutputChange = change;
				}
			}

			if (changesCallback != nullptr) {
				changesCallback();
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceAdded(const wchar_t* deviceId) override {
			auto lock = getLock();
			if (activeDevices.find(deviceId) != activeDevices.end()) {
				return S_OK;
			}

			activeDevices.insert(deviceId);
			changes.stateChanged.insert(deviceId);
			changes.removed.erase(deviceId);

			if (changesCallback != nullptr) {
				changesCallback();
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceRemoved(const wchar_t* deviceId) override {
			auto lock = getLock();
			if (activeDevices.find(deviceId) == activeDevices.end()) {
				return S_OK;
			}

			activeDevices.erase(deviceId);
			changes.removed.insert(deviceId);
			changes.stateChanged.erase(deviceId);

			if (changesCallback != nullptr) {
				changesCallback();
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(const wchar_t* deviceId, DWORD newState) override {
			auto lock = getLock();

			if (newState == DEVICE_STATE_ACTIVE) {
				if (activeDevices.find(deviceId) != activeDevices.end()) {
					return S_OK;
				}

				activeDevices.insert(deviceId);
				changes.stateChanged.insert(deviceId);
				changes.removed.erase(deviceId);
			} else {
				if (activeDevices.find(deviceId) == activeDevices.end()) {
					return S_OK;
				}

				activeDevices.insert(deviceId);
				changes.removed.insert(deviceId);
				changes.stateChanged.erase(deviceId);
			}

			if (changesCallback != nullptr) {
				changesCallback();
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(const wchar_t* deviceId, const PROPERTYKEY key) override {
			auto lock = getLock();

			if (activeDevices.find(deviceId) == activeDevices.end()) {
				return S_OK;
			}

			changes.stateChanged.insert(deviceId);

			if (changesCallback != nullptr) {
				changesCallback();
			}

			return S_OK;
		}

	private:
		std::lock_guard<std::mutex> getLock() {
			return std::lock_guard<std::mutex>{ mut };
		}
	};
}
