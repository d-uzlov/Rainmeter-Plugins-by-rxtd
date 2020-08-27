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

#include "GenericCoTaskMemWrapper.h"
#include "PropertyStoreWrapper.h"

using namespace utils;

MediaDeviceWrapper::MediaDeviceWrapper(InitFunction initFunction) :
	GenericComWrapper(std::move(initFunction)) {
}

MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
	if (!isValid()) {
		return { };
	}

	GenericCoTaskMemWrapper<wchar_t> idWrapper{
		[&](auto ptr) {
			lastResult = getPointer()->GetId(ptr);
			return lastResult == S_OK;
		}
	};
	if (!idWrapper.isValid()) {
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
	deviceInfo.id = idWrapper.getPointer();
	deviceInfo.fullFriendlyName = props.readProperty(PKEY_Device_FriendlyName);
	deviceInfo.desc = props.readProperty(PKEY_Device_DeviceDesc);
	deviceInfo.name = props.readProperty(PKEY_DeviceInterface_FriendlyName);

	return deviceInfo;
}

IAudioClientWrapper MediaDeviceWrapper::openAudioClient() {
	return IAudioClientWrapper{
		[&](auto ptr) {
			lastResult = typedQuery(&IMMDevice::Activate, ptr, CLSCTX_INPROC_SERVER, nullptr);
			return lastResult == S_OK;
		}
	};
}
