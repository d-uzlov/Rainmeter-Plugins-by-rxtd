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
#include "Port.h"
#include "windows-wrappers/MediaDeviceWrapper.h"


namespace rxtd::audio_analyzer {
	class AudioEnumeratorWrapper {
	public:
		struct DeviceInfo {
			string name;
			string id;
		};

	private:
		bool objectIsValid = true;

		utils::Rainmeter::Logger& logger;

		utils::GenericComWrapper<IMMDeviceEnumerator> audioEnumeratorHandle;

		string deviceListLegacy;
		string deviceList2;

	public:

		AudioEnumeratorWrapper(utils::Rainmeter::Logger& logger);
		bool isValid() const;

		std::optional<utils::MediaDeviceWrapper> getDevice(const string& deviceID, Port port);
		std::optional<utils::MediaDeviceWrapper> getDefaultDevice(Port port);
		string getDefaultDeviceId(Port port);

		const string& getDeviceListLegacy() const;
		const string& getDeviceList2() const;
		void updateDeviceList(Port port);

	private:
	};
}
