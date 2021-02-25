/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MediaDeviceListNotificationClient.h"

using rxtd::audio_analyzer::wasapi_wrappers::implementations::MediaDeviceListNotificationClient;

void MediaDeviceListNotificationClient::init(MediaDeviceEnumerator& enumerator) {
	auto lock = getLock();
	enumerator.ref().RegisterEndpointNotificationCallback(this);

	for (auto& dev : enumerator.getActiveDevices(MediaDeviceType::eINPUT)) {
		activeDevices.insert(dev.getId() % own());
	}

	for (auto& dev : enumerator.getActiveDevices(MediaDeviceType::eOUTPUT)) {
		activeDevices.insert(dev.getId() % own());
	}

	defaultInputId = enumerator.getDefaultDevice(MediaDeviceType::eINPUT).value_or(MediaDeviceHandle{}).getId();
	defaultOutputId = enumerator.getDefaultDevice(MediaDeviceType::eOUTPUT).value_or(MediaDeviceHandle{}).getId();
}

void MediaDeviceListNotificationClient::deinit(MediaDeviceEnumerator& enumerator) {
	enumerator.ref().UnregisterEndpointNotificationCallback(this);
}

HRESULT MediaDeviceListNotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, const wchar_t* deviceId) {
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

HRESULT MediaDeviceListNotificationClient::OnDeviceAdded(const wchar_t* deviceId) {
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

HRESULT MediaDeviceListNotificationClient::OnDeviceRemoved(const wchar_t* deviceId) {
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

HRESULT MediaDeviceListNotificationClient::OnDeviceStateChanged(const wchar_t* deviceId, DWORD newState) {
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

HRESULT MediaDeviceListNotificationClient::OnPropertyValueChanged(const wchar_t* deviceId, const PROPERTYKEY key) {
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
