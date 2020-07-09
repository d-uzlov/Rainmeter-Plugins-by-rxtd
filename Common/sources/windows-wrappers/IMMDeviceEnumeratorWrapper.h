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
		index lastResult = { };

	public:
		IMMDeviceEnumeratorWrapper();

		index getLastResult() const;

		MediaDeviceWrapper getDeviceByID(MediaDeviceType type, const string& id);
		
		MediaDeviceWrapper getDefaultDevice(MediaDeviceType type);

		std::vector<MediaDeviceWrapper> getActiveDevices(MediaDeviceType type);
		std::vector<MediaDeviceWrapper> getAllDevices(MediaDeviceType type);

	private:
		std::vector<MediaDeviceWrapper> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
