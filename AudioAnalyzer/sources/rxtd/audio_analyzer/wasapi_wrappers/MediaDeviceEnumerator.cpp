/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MediaDeviceEnumerator.h"

using rxtd::audio_analyzer::wasapi_wrappers::MediaDeviceEnumerator;
using rxtd::audio_analyzer::wasapi_wrappers::MediaDeviceHandle;

MediaDeviceEnumerator::MediaDeviceEnumerator() : GenericComWrapper(
	[](auto ptr) {
		return S_OK == CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IMMDeviceEnumerator),
			reinterpret_cast<void**>(ptr)
		);
	}
) {}

std::optional<MediaDeviceHandle> MediaDeviceEnumerator::getDeviceByID(const string& id) {
	try {
		return MediaDeviceHandle{
			[&](auto ptr) {
				throwOnError(ref().GetDevice(id.c_str(), ptr), L"Device with specified id is not found");
				return true;
			},
			id
		};
	} catch (ComException&) {
		return {};
	}
}

std::optional<MediaDeviceHandle> MediaDeviceEnumerator::getDefaultDevice(MediaDeviceType type) {
	try {
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
	} catch (ComException&) {
		return {};
	}
}

std::vector<MediaDeviceHandle> MediaDeviceEnumerator::getActiveDevices(MediaDeviceType type) {
	return getCollection(type, DEVICE_STATE_ACTIVE);
}

std::vector<MediaDeviceHandle> MediaDeviceEnumerator::getCollection(
	MediaDeviceType type, uint32_t deviceStateMask
) {
	GenericComWrapper<IMMDeviceCollection> collection{
		[&](auto ptr) {
			return S_OK == ref().EnumAudioEndpoints(
				type == MediaDeviceType::eOUTPUT ? eRender : eCapture,
				deviceStateMask,
				ptr
			);
		}
	};

	static_assert(std::is_same<UINT, uint32_t>::value);

	uint32_t devicesCountUINT;
	collection.ref().GetCount(&devicesCountUINT);
	const index devicesCount = devicesCountUINT;

	std::vector<MediaDeviceHandle> result;
	for (index i = 0; i < devicesCount; ++i) {
		MediaDeviceHandle device{
			[&](auto ptr) {
				return S_OK == collection.ref().Item(UINT(i), ptr);
			},
			type
		};

		result.push_back(std::move(device));
	}

	return result;
}
