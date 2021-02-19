/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

 // my-windows must be before any WINAPI include
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <mmdeviceapi.h>

#include "MediaDeviceHandle.h"
#include "winapi-wrappers/GenericComWrapper.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	class MediaDeviceEnumerator : public common::winapi_wrappers::GenericComWrapper<IMMDeviceEnumerator> {
	public:
		MediaDeviceEnumerator();

		[[nodiscard]]
		std::optional<MediaDeviceHandle> getDeviceByID(const string& id);

		[[nodiscard]]
		std::optional<MediaDeviceHandle> getDefaultDevice(MediaDeviceType type);

		[[nodiscard]]
		std::vector<MediaDeviceHandle> getActiveDevices(MediaDeviceType type);

	private:
		[[nodiscard]]
		std::vector<MediaDeviceHandle> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
