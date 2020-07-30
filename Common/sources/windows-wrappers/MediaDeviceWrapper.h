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
#include "IAudioClientWrapper.h"
#include "MediaDeviceType.h"

namespace rxtd::utils {
	class MediaDeviceWrapper : public GenericComWrapper<IMMDevice> {
	public:
		struct DeviceInfo {
			string id;
			string name;
			string desc;
			string fullFriendlyName;
		};

	private:
		MediaDeviceType type{ };
		index lastResult = { };

	public:
		MediaDeviceWrapper() = default;

		explicit MediaDeviceWrapper(MediaDeviceType type, InitFunction initFunction);

		[[nodiscard]]
		DeviceInfo readDeviceInfo();
		
		[[nodiscard]]
		string readDeviceId();

		[[nodiscard]]
		IAudioClientWrapper openAudioClient();

		[[nodiscard]]
		index getLastResult() const;

		[[nodiscard]]
		MediaDeviceType getType() const;

		[[nodiscard]]
		bool isDeviceActive();
	};
}
