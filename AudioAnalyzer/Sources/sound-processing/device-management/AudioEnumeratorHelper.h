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
		bool valid = true;

		utils::Rainmeter::Logger logger;

		utils::IMMDeviceEnumeratorWrapper enumeratorWrapper;

		// IDs of all existing devices
		std::set<string> inputDevicesIDs;
		std::set<string> outputDevicesIDs;

		string deviceStringInput;
		string deviceStringOutput;

		string deviceStringLegacy;

	public:
		AudioEnumeratorHelper(utils::Rainmeter::Logger logger);

		bool isValid() const;

		std::optional<utils::MediaDeviceWrapper> getDevice(const string& deviceID);
		std::optional<utils::MediaDeviceWrapper> getDefaultDevice(utils::MediaDeviceType type);

		std::optional<utils::MediaDeviceType> getDeviceType(const string& deviceID);
		const string& getDeviceListInput() const;
		const string& getDeviceListOutput() const;
		void updateDeviceStrings();
		void makeDeviceString(utils::MediaDeviceType type, string& result);

		void updateDeviceStringLegacy(utils::MediaDeviceType type);
		const string& getDeviceListLegacy() const {
			return deviceStringLegacy;
		}

		utils::IMMDeviceEnumeratorWrapper& getWrapper() {
			return enumeratorWrapper;
		}
	private:
		void updateDeviceLists();
		std::set<string> readDeviceIdList(utils::MediaDeviceType type);
	};
}
