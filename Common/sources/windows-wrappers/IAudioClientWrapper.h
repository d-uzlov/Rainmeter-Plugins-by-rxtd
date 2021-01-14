/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>
#include "IAudioCaptureClientWrapper.h"
#include "WaveFormat.h"
#include "MediaDeviceType.h"
#include "audiopolicy.h"
#include "GenericCoTaskMemWrapper.h"

namespace rxtd::utils {
	class IAudioClientWrapper : public GenericComWrapper<IAudioClient> {
		HRESULT lastResult = { };

		GenericCoTaskMemWrapper<WAVEFORMATEX> nativeFormat{ };
		WaveFormat format;
		bool formatIsValid = false;

		MediaDeviceType type{ };
		IAudioCaptureClientWrapper::Type formatType{ };

	public:
		IAudioClientWrapper() = default;

		template <typename InitFunction>
		IAudioClientWrapper(InitFunction initFunction) : GenericComWrapper(std::move(initFunction)) {
		}

		static index get1sec100nsUnits() {
			return 1000'000'0;
		}

		IAudioCaptureClientWrapper openCapture();

		// check lastResult after this function
		// if (lastResult == S_OK) then you can call initShared on another instance of IAudioClientWrapper
		// if (lastResult == AUDCLNT_E_DEVICE_IN_USE) then device is in exclusive mode
		// other errors may happen
		// the meaning of this function is to reduce memory leaks caused by WASAPI exclusive mode
		//
		// the object is unusable after this function, so create separate object before calling this
		void testExclusive();

		void initShared(index bufferSize100nsUnits);

		[[nodiscard]]
		HRESULT getLastResult() const {
			return lastResult;
		}

		[[nodiscard]]
		bool isFormatValid() const {
			return formatIsValid;
		}

		void readFormat();

		[[nodiscard]]
		WaveFormat getFormat() const {
			return format;
		}

		[[nodiscard]]
		auto getType() const {
			return type;
		}

		template <typename Interface>
		GenericComWrapper<Interface> getInterface() {
			static_assert(
				std::is_base_of<IAudioClock, Interface>::value
				|| std::is_base_of<IAudioRenderClient, Interface>::value
				|| std::is_base_of<IAudioSessionControl, Interface>::value
				|| std::is_base_of<IAudioStreamVolume, Interface>::value
				|| std::is_base_of<IChannelAudioVolume, Interface>::value
				|| std::is_base_of<ISimpleAudioVolume, Interface>::value,
				"Interface is not supported by IAudioClient"
			);

			return {
				[&](auto ptr) {
					lastResult = typedQuery(&IAudioClient::GetService, ptr);
					return lastResult == S_OK;
				}
			};
		}
	};
}
