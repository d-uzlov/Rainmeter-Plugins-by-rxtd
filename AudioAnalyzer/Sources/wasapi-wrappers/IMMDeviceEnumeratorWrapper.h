/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <mmdeviceapi.h>

#include "MediaDeviceWrapper.h"
#include "winapi-wrappers/GenericComWrapper.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	enum class MediaDeviceType;

	class IMMDeviceEnumeratorWrapper : public common::winapi_wrappers::GenericComWrapper<IMMDeviceEnumerator> {
	public:
		IMMDeviceEnumeratorWrapper();

		[[nodiscard]]
		std::optional<MediaDeviceWrapper> getDeviceByID(const string& id);

		[[nodiscard]]
		std::optional<MediaDeviceWrapper> getDefaultDevice(MediaDeviceType type);

		[[nodiscard]]
		std::vector<MediaDeviceWrapper> getActiveDevices(MediaDeviceType type);

	private:
		[[nodiscard]]
		std::vector<MediaDeviceWrapper> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
