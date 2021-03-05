// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

#include <functional>
#include <mutex>
#include <set>

// my-windows must be before any WINAPI include
#include "rxtd/my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <mmdeviceapi.h>

#include "rxtd/audio_analyzer/wasapi_wrappers/MediaDeviceEnumerator.h"
#include "rxtd/winapi_wrappers/IUnknownImpl.h"

namespace rxtd::audio_analyzer::wasapi_wrappers::implementations {
	class MediaDeviceListNotificationClient : public winapi_wrappers::IUnknownImpl<IMMNotificationClient> {
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
		MediaDeviceEnumerator enumerator;
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

	public:
		void init(MediaDeviceEnumerator& enumerator);

		void deinit(MediaDeviceEnumerator& enumerator);

		// callback must not call any methods of the object
		//		including #takeChanges()
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

		HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, const wchar_t* deviceId) override;

		HRESULT STDMETHODCALLTYPE OnDeviceAdded(const wchar_t* deviceId) override;

		HRESULT STDMETHODCALLTYPE OnDeviceRemoved(const wchar_t* deviceId) override;

		HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(const wchar_t* deviceId, DWORD newState) override;

		HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(const wchar_t* deviceId, const PROPERTYKEY key) override;

	private:
		std::lock_guard<std::mutex> getLock() {
			return std::lock_guard<std::mutex>{ mut };
		}
	};
}
