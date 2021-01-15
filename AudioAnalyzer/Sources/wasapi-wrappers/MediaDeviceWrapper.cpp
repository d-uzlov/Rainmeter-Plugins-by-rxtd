/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

// initguid.h must be the first include
// source: https://social.msdn.microsoft.com/Forums/Windowsapps/en-US/00853e55-51dd-46bc-bceb-04c0c2e5cc06/unresolved-external-symbols?forum=mediafoundationdevelopment
#include <initguid.h>

#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>

#include "winapi-wrappers/PropertyStoreWrapper.h"

using namespace utils;

MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() noexcept(false) {
	if (!isValid()) {
		return {};
	}

	PropertyStoreWrapper props{
		[&](auto ptr) {
			throwOnError(ref().OpenPropertyStore(STGM_READ, ptr), L"IMMDevice.OpenPropertyStore() in MediaDeviceWrapper::readDeviceInfo()");
			return true;
		}
	};

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
			throwOnError(typedQuery(&IMMDevice::Activate, ptr, CLSCTX_INPROC_SERVER, nullptr), L"IMMDevice.Activate(IAudioClient) in MediaDeviceWrapper::openAudioClient()");
			return true;
		},
		type
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
