/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IMMDeviceEnumeratorWrapper.h"

using namespace utils;

IMMDeviceEnumeratorWrapper::IMMDeviceEnumeratorWrapper() : GenericComWrapper(
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

MediaDeviceWrapper IMMDeviceEnumeratorWrapper::getDeviceByID(const string& id) {
	return MediaDeviceWrapper{
		[&](auto ptr) {
			return S_OK == ref().GetDevice(id.c_str(), ptr);
		}
	};
}

MediaDeviceWrapper IMMDeviceEnumeratorWrapper::getDefaultDevice(MediaDeviceType type) {
	return MediaDeviceWrapper{
		[&](auto ptr) {
			return S_OK == ref().GetDefaultAudioEndpoint(
				type == MediaDeviceType::eOUTPUT ? eRender : eCapture,
				eConsole,
				ptr
			);
		}
	};
}

std::vector<MediaDeviceWrapper> IMMDeviceEnumeratorWrapper::getActiveDevices(MediaDeviceType type) {
	return getCollection(type, DEVICE_STATE_ACTIVE);
}

std::vector<MediaDeviceWrapper> IMMDeviceEnumeratorWrapper::getCollection(
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

	std::vector<MediaDeviceWrapper> result;
	for (index i = 0; i < devicesCount; ++i) {
		MediaDeviceWrapper device{
			[&](auto ptr) {
				return S_OK == collection.ref().Item(UINT(i), ptr);
			}
		};

		result.push_back(std::move(device));
	}

	return result;
}
