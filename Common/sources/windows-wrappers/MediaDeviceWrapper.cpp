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

	DeviceInfo deviceInfo;

	wchar_t* idPtr = nullptr;
	lastResult = getPointer()->GetId(&idPtr);
	if (lastResult != S_OK) {
		return { };
	}

	deviceInfo.id = idPtr;
	CoTaskMemFree(idPtr);

	PropertyStoreWrapper props{
		[&](auto ptr) {
			return S_OK == getPointer()->OpenPropertyStore(STGM_READ, ptr);
		}
	};
	if (!props.isValid()) {
		return { };
	}

	deviceInfo.fullFriendlyName = props.readProperty(PKEY_Device_FriendlyName);
	deviceInfo.desc = props.readProperty(PKEY_Device_DeviceDesc);
	deviceInfo.name = props.readProperty(PKEY_DeviceInterface_FriendlyName);

	return deviceInfo;
}

IAudioClientWrapper MediaDeviceWrapper::openAudioClient() {
	return IAudioClientWrapper{
		[&](auto ptr) {
			lastResult = getPointer()->Activate(
				IID_IAudioClient,
				CLSCTX_INPROC_SERVER,
				nullptr,
				reinterpret_cast<void**>(ptr)
			);
			return lastResult == S_OK;
		}
	};
}
