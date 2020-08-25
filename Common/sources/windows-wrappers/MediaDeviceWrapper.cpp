/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>
#include "PropertyStoreWrapper.h"

static const IID IID_IAudioClient = __uuidof(IAudioClient);

using namespace utils;

MediaDeviceWrapper::MediaDeviceWrapper(InitFunction initFunction) :
	GenericComWrapper(std::move(initFunction)) {
}

MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
	if (!isValid()) {
		return { };
	}

	PropertyStoreWrapper props{
		[&](auto ptr) {
			return S_OK == getPointer()->OpenPropertyStore(STGM_READ, ptr);
		}
	};
	if (!props.isValid()) {
		return { };
	}

	DeviceInfo deviceInfo;
	deviceInfo.id = readDeviceId();
	deviceInfo.fullFriendlyName = props.readProperty(PKEY_Device_FriendlyName);
	deviceInfo.desc = props.readProperty(PKEY_Device_DeviceDesc);
	deviceInfo.name = props.readProperty(PKEY_DeviceInterface_FriendlyName);

	return deviceInfo;
}

string MediaDeviceWrapper::readDeviceId() {
	if (!isValid()) {
		return { };
	}

	wchar_t* resultCString = nullptr;
	lastResult = getPointer()->GetId(&resultCString);
	if (lastResult != S_OK) {
		return { };
	}
	string id = resultCString;

	CoTaskMemFree(resultCString);

	return id;
}

IAudioClientWrapper MediaDeviceWrapper::openAudioClient() {
	return IAudioClientWrapper{
		[&](auto ptr) {
			lastResult = getPointer()->Activate(
				IID_IAudioClient,
				CLSCTX_ALL,
				nullptr,
				reinterpret_cast<void**>(ptr)
			);
			return lastResult == S_OK;
		}
	};
}

bool MediaDeviceWrapper::isDeviceActive() {
	if (!isValid()) {
		return false;
	}

	// static_assert(std::is_same<DWORD, uint32_t>::value);
	// ...
	// unsigned != unsigned long
	// I wish I could use proper types everywhere :(

	DWORD state;
	const auto result = getPointer()->GetState(&state);
	if (result != S_OK) {
		return false;
	}

	return state == DEVICE_STATE_ACTIVE;
}
