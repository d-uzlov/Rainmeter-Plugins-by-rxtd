/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "MediaDeviceWrapper.h"

namespace rxtd::utils {
	enum class MediaDeviceType;

	class IMMDeviceEnumeratorWrapper : public GenericComWrapper<IMMDeviceEnumerator> {
	public:
		IMMDeviceEnumeratorWrapper();

		[[nodiscard]]
		MediaDeviceWrapper getDeviceByID(MediaDeviceType type, const string& id);

		[[nodiscard]]
		MediaDeviceWrapper getDefaultDevice(MediaDeviceType type);

		[[nodiscard]]
		std::vector<MediaDeviceWrapper> getActiveDevices(MediaDeviceType type);
		
		[[nodiscard]]
		std::vector<MediaDeviceWrapper> getAllDevices(MediaDeviceType type);

	private:
		[[nodiscard]]
		std::vector<MediaDeviceWrapper> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
