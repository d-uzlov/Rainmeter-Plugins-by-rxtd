// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

// initguid.h must be the first include
// source: https://social.msdn.microsoft.com/Forums/Windowsapps/en-US/00853e55-51dd-46bc-bceb-04c0c2e5cc06/unresolved-external-symbols?forum=mediafoundationdevelopment
#include <initguid.h>

#include "MediaDeviceHandle.h"

#include <functiondiscoverykeys_devpkey.h>

#include "rxtd/winapi_wrappers/PropertyStoreWrapper.h"

using rxtd::audio_analyzer::wasapi_wrappers::MediaDeviceHandle;
using rxtd::audio_analyzer::wasapi_wrappers::AudioClientHandle;

MediaDeviceHandle::DeviceInfo MediaDeviceHandle::readDeviceInfo() noexcept(false) {
	if (!isValid()) {
		return {};
	}

	winapi_wrappers::PropertyStoreWrapper props{
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

AudioClientHandle MediaDeviceHandle::openAudioClient() noexcept(false) {
	return AudioClientHandle{
		[&](auto ptr) {
			typedQuery(&IMMDevice::Activate, ptr, L"IMMDevice.Activate(IAudioClient) in MediaDeviceWrapper::openAudioClient()", CLSCTX_INPROC_SERVER, nullptr);
		},
		type
	};
}

rxtd::sview MediaDeviceHandle::convertFormFactor(std::optional<EndpointFormFactor> valueOpt) noexcept {
	switch (valueOpt.value_or(UnknownFormFactor)) {
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
	return {};
}

void MediaDeviceHandle::readType() noexcept(false) {
	auto endpoint = GenericComWrapper<IMMEndpoint>{
		[&](auto ptr) {
			throwOnError(ref().QueryInterface(ptr), L"IMMDevice.QueryInterface(IMMEndpoint)");
			return true;
		}
	};
	EDataFlow flow{};
	throwOnError(endpoint.ref().GetDataFlow(&flow), L"IMMEndpoint.GetDataFlow()");
	switch (flow) {
	case eRender:
		type = MediaDeviceType::eOUTPUT;
		break;
	case eCapture:
		type = MediaDeviceType::eINPUT;
		break;

	case eAll:
	case EDataFlow_enum_count:
		throw ComException{ -1, L"invalid EDataFlow value from IMMEndpoint.GetDataFlow()" };
	}
}

void MediaDeviceHandle::readId() noexcept(false) {
	winapi_wrappers::GenericCoTaskMemWrapper<wchar_t> idWrapper{
		[&](auto ptr) {
			throwOnError(ref().GetId(ptr), L"IMMDevice.GetId()");
			return true;
		}
	};
	id = idWrapper.getPointer();
	if (id.empty()) {
		throw ComException{ -1, L"invalid empty value for device id from IMMDevice.GetId()" };
	}
}
