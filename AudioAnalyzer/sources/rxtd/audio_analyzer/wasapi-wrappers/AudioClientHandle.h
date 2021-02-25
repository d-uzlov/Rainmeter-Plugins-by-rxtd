/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

// my-windows must be before any WINAPI include
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Audioclient.h>

#include "AudioCaptureClient.h"
#include "audiopolicy.h"
#include "MediaDeviceType.h"
#include "WaveFormat.h"
#include "rxtd/winapi-wrappers/GenericComWrapper.h"
#include "rxtd/winapi-wrappers/GenericCoTaskMemWrapper.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	class AudioClientHandle : public common::winapi_wrappers::GenericComWrapper<IAudioClient> {
		common::winapi_wrappers::GenericCoTaskMemWrapper<WAVEFORMATEX> nativeFormat{};
		WaveFormat format;

		MediaDeviceType type{};
		AudioCaptureClient::Type formatType{};

	public:
		AudioClientHandle() = default;

		template<typename InitFunction>
		AudioClientHandle(InitFunction initFunction, MediaDeviceType type) : GenericComWrapper(std::move(initFunction)), type(type) {
			readFormat();
		}

		// This method does nothing except connecting to device
		// If it fails by throwing ComException with error code AUDCLNT_E_DEVICE_IN_USE
		// then device is in exclusive mode
		// You should wait for device to exit exclusive mode to avoid memory leak
		//
		// the object is unusable after this function, so create separate object before calling this
		void testExclusive() noexcept(false);

		AudioCaptureClient openCapture(double bufferSizeSec) noexcept(false);

		GenericComWrapper<IAudioRenderClient> openRender() noexcept(false);

		[[nodiscard]]
		const WaveFormat& getFormat() const {
			return format;
		}

		[[nodiscard]]
		MediaDeviceType getType() const {
			return type;
		}

		template<typename Interface>
		GenericComWrapper<Interface> getInterface() noexcept(false) {
			sview source;
			if constexpr (std::is_same_v<IAudioClock, Interface>) {
				source = L"IAudioClient.GetService(IAudioClock)";
			} else if constexpr (std::is_same_v<IAudioRenderClient, Interface>) {
				source = L"IAudioClient.GetService(IAudioRenderClient)";
			} else if constexpr (std::is_same_v<IAudioSessionControl, Interface>) {
				source = L"IAudioClient.GetService(IAudioSessionControl)";
			} else if constexpr (std::is_same_v<IAudioStreamVolume, Interface>) {
				source = L"IAudioClient.GetService(IAudioStreamVolume)";
			} else if constexpr (std::is_same_v<IChannelAudioVolume, Interface>) {
				source = L"IAudioClient.GetService(IChannelAudioVolume)";
			} else if constexpr (std::is_same_v<ISimpleAudioVolume, Interface>) {
				source = L"IAudioClient.GetService(ISimpleAudioVolume)";
			} else {
				static_assert(false, "Interface is not supported by IAudioClient");
			}

			return {
				[&](auto ptr) {
					typedQuery(&IAudioClient::GetService, ptr, source);
				}
			};
		}

	private:
		void readFormat() noexcept(false);
	};
}
