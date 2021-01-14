/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

// this must be the first include
// source: https://social.msdn.microsoft.com/Forums/Windowsapps/en-US/00853e55-51dd-46bc-bceb-04c0c2e5cc06/unresolved-external-symbols?forum=mediafoundationdevelopment
#include <initguid.h>

#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>

#include "GenericCoTaskMemWrapper.h"
#include "PropertyStoreWrapper.h"

using namespace utils;

string MediaDeviceWrapper::readId() {
	GenericCoTaskMemWrapper<wchar_t> idWrapper{
		[&](auto ptr) {
			lastResult = ref().GetId(ptr);
			return lastResult == S_OK;
		}
	};
	return idWrapper.isValid() ? idWrapper.getPointer() : string{};
}

MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
	if (!isValid()) {
		return {};
	}

	PropertyStoreWrapper props{
		[&](auto ptr) {
			return S_OK == ref().OpenPropertyStore(STGM_READ, ptr);
		}
	};
	if (!props.isValid()) {
		return {};
	}

	DeviceInfo deviceInfo;
	deviceInfo.fullFriendlyName = props.readPropertyString(PKEY_Device_FriendlyName).value_or(L"");
	deviceInfo.desc = props.readPropertyString(PKEY_Device_DeviceDesc).value_or(L"");
	deviceInfo.name = props.readPropertyString(PKEY_DeviceInterface_FriendlyName).value_or(L"");

	deviceInfo.formFactor = convertFormFactor(
		props.readPropertyInt<EndpointFormFactor>(PKEY_AudioEndpoint_FormFactor)
	);

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

sview MediaDeviceWrapper::convertFormFactor(std::optional<EndpointFormFactor> valueOpt) {
	if (!valueOpt.has_value()) {
		return L"";
	}

	switch (valueOpt.value()) {
	case RemoteNetworkDevice: return L"RemoteNetworkDevice";
	case Speakers: return L"Speakers";
	case LineLevel: return L"LineLevel";
	case Headphones: return L"Headphones";
	case Microphone: return L"Microphone";
	case Headset: return L"Headset";
	case Handset: return L"Handset";
	case UnknownDigitalPassthrough: return L"UnknownDigitalPassthrough";
	case SPDIF: return L"SPDIF";
	case DigitalAudioDisplayDevice: return L"DigitalAudioDisplayDevice";

	case UnknownFormFactor:
	case EndpointFormFactor_enum_count:
		break;
	}
	return L"";
}
