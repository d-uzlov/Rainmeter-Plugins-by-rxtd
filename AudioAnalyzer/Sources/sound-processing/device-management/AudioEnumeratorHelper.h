/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "RainmeterWrappers.h"
#include "windows-wrappers/MediaDeviceWrapper.h"
#include <set>
#include "windows-wrappers/IMMDeviceEnumeratorWrapper.h"

namespace rxtd::audio_analyzer {
	class AudioEnumeratorHelper {
		utils::IMMDeviceEnumeratorWrapper enumeratorWrapper;

	public:
		[[nodiscard]]
		bool isValid() const {
			return enumeratorWrapper.isValid();
		}

		[[nodiscard]]
		std::optional<utils::MediaDeviceWrapper> getDevice(const string& deviceID);
		[[nodiscard]]
		std::optional<utils::MediaDeviceWrapper> getDefaultDevice(utils::MediaDeviceType type);

		string makeDeviceString(utils::MediaDeviceType type);
		string legacy_makeDeviceString(utils::MediaDeviceType type);

	private:
		[[nodiscard]]
		std::set<string> readDeviceIdList(utils::MediaDeviceType type);
	};
}
