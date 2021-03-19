// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "MediaDeviceEnumerator.h"

using rxtd::audio_analyzer::wasapi_wrappers::MediaDeviceEnumerator;
using rxtd::audio_analyzer::wasapi_wrappers::MediaDeviceHandle;

MediaDeviceEnumerator::MediaDeviceEnumerator() : GenericComWrapper(
	[](auto ptr) {
		throwOnError(CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IMMDeviceEnumerator),
			reinterpret_cast<void**>(ptr)
		), L"MediaDeviceEnumerator::ctor");
	}
) {}

MediaDeviceHandle MediaDeviceEnumerator::getDeviceByID(const string& id) {
	return MediaDeviceHandle{
		[&](auto ptr) {
			throwOnError(ref().GetDevice(id.c_str(), ptr), L"Device with specified id is not found");
			return true;
		},
		id
	};
}

MediaDeviceHandle MediaDeviceEnumerator::getDefaultDevice(MediaDeviceType type) {
	return MediaDeviceHandle{
		[&](auto ptr) {
			throwOnError(
				ref().GetDefaultAudioEndpoint(
					type == MediaDeviceType::eOUTPUT ? eRender : eCapture,
					eConsole,
					ptr
				), L"Default audio device is not found"
			);
			return true;
		},
		type
	};
}

std::vector<MediaDeviceHandle> MediaDeviceEnumerator::getActiveDevices(MediaDeviceType type) {
	return getCollection(type, DEVICE_STATE_ACTIVE);
}

std::vector<MediaDeviceHandle> MediaDeviceEnumerator::getCollection(
	MediaDeviceType type, uint32_t deviceStateMask
) {
	GenericComWrapper<IMMDeviceCollection> collection{
		[&](auto ptr) {
			throwOnError(
				ref().EnumAudioEndpoints(
					type == MediaDeviceType::eOUTPUT ? eRender : eCapture,
					deviceStateMask,
					ptr
				), L"MediaDeviceEnumerator::getCollection::IMMDeviceCollection::ctor"
			);
		}
	};

	static_assert(std::is_same<UINT, uint32_t>::value);

	uint32_t devicesCountUINT;
	throwOnError(collection.ref().GetCount(&devicesCountUINT), L"MediaDeviceEnumerator::getCollection::IMMDeviceCollection::GetCount");
	const index devicesCount = devicesCountUINT;

	std::vector<MediaDeviceHandle> result;
	for (index i = 0; i < devicesCount; ++i) {
		try {
			MediaDeviceHandle device{
				[&](auto ptr) {
					return S_OK == collection.ref().Item(static_cast<UINT>(i), ptr);
				},
				type
			};

			result.push_back(std::move(device));
		} catch (ComException&) {
			// ignore all COM exceptions for list forming
		}
	}

	return result;
}
