/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "windows-wrappers/GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "RainmeterWrappers.h"
#include "windows-wrappers/MediaDeviceWrapper.h"
#include <set>


namespace rxtd::audio_analyzer {
	class AudioEnumeratorWrapper {
	private:
		bool valid = true;

		utils::Rainmeter::Logger& logger;

		utils::GenericComWrapper<IMMDeviceEnumerator> audioEnumeratorHandle;

		// IDs of all available devices
		std::set<string> inputDevices;
		std::set<string> outputDevices;

		string deviceListLegacy;
		string deviceListActive;

	public:
		AudioEnumeratorWrapper(utils::Rainmeter::Logger& logger);

		bool isValid() const;

		std::optional<utils::MediaDeviceWrapper> getDevice(const string& deviceID);
		std::optional<utils::MediaDeviceWrapper> getDefaultDevice(utils::MediaDeviceType port);
		string getDefaultDeviceId(utils::MediaDeviceType port);

		std::optional<utils::MediaDeviceType> getDeviceType(const string& deviceID);
		const string& getDeviceListLegacy() const;
		const string& getActiveDeviceList() const;
		// TODO make 2 lists for input and output devices
		void updateActiveDeviceList(utils::MediaDeviceType port);

	private:
		void updateDeviceLists();
		std::set<string> readDeviceList(utils::MediaDeviceType port);
	};
}
