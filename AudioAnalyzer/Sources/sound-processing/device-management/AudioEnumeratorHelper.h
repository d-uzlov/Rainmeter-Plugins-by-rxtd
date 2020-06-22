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
	private:
		bool valid = true;

		utils::Rainmeter::Logger& logger;

		utils::IMMDeviceEnumeratorWrapper enumeratorWrapper;

		// IDs of all available devices
		std::set<string> inputDevices;
		std::set<string> outputDevices;

		string deviceStringLegacy;
		string deviceStringInput;
		string deviceStringOutput;

	public:
		AudioEnumeratorHelper(utils::Rainmeter::Logger& logger);

		bool isValid() const;

		std::optional<utils::MediaDeviceWrapper> getDevice(const string& deviceID);
		std::optional<utils::MediaDeviceWrapper> getDefaultDevice(utils::MediaDeviceType type);
		string getDefaultDeviceId(utils::MediaDeviceType type);

		std::optional<utils::MediaDeviceType> getDeviceType(const string& deviceID);
		const string& getDeviceListLegacy() const;
		const string& getDeviceListInput() const;
		const string& getDeviceListOutput() const;
		void updateDeviceStringLegacy(utils::MediaDeviceType type);
		void updateDeviceStrings();
		string makeDeviceString(utils::MediaDeviceType type);

	private:
		void updateDeviceLists();
		std::set<string> readDeviceList(utils::MediaDeviceType type);
	};
}
