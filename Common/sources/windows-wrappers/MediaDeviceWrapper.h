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
#include <endpointvolume.h>
#include "IAudioClientWrapper.h"

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
		index lastResult = { };

	public:
		MediaDeviceWrapper() = default;

		explicit MediaDeviceWrapper(InitFunction initFunction);

		[[nodiscard]]
		DeviceInfo readDeviceInfo();

		[[nodiscard]]
		IAudioClientWrapper openAudioClient();

		[[nodiscard]]
		index getLastResult() const {
			return lastResult;
		}

		template <typename Interface>
		GenericComWrapper<Interface> activateFor() {
			static_assert(
				std::is_base_of<IAudioClient, Interface>::value
				|| std::is_base_of<IAudioEndpointVolume, Interface>::value
				|| std::is_base_of<IAudioMeterInformation, Interface>::value
				|| std::is_base_of<IAudioSessionManager, Interface>::value
				|| std::is_base_of<IAudioSessionManager2, Interface>::value
				|| std::is_base_of<IDeviceTopology, Interface>::value,
				"Interface is not supported by IMMDevice"
			);

			return {
				[&](auto ptr) {
					lastResult = typedQuery(&IMMDevice::Activate, ptr, CLSCTX_INPROC_SERVER, nullptr);
					return lastResult == S_OK;
				}
			};
		}
	};
}
