/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <functional>

#include "rxtd/audio_analyzer/Version.h"
#include "rxtd/audio_analyzer/sound-processing/ChannelMixer.h"
#include "rxtd/audio_analyzer/wasapi-wrappers/AudioCaptureClient.h"
#include "rxtd/audio_analyzer/wasapi-wrappers/MediaDeviceEnumerator.h"
#include "rxtd/audio_analyzer/wasapi-wrappers/MediaDeviceHandle.h"
#include "rxtd/audio_analyzer/wasapi-wrappers/implementations/AudioSessionEventsImpl.h"
#include "rxtd/rainmeter/Logger.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
		using Logger = common::rainmeter::Logger;

		template<typename T>
		using GenericComWrapper = common::winapi_wrappers::GenericComWrapper<T>;

		struct SourceDesc {
			enum class Type {
				eDEFAULT_INPUT,
				eDEFAULT_OUTPUT,
				eID,
			} type{};

			string id;

			friend bool operator==(const SourceDesc& lhs, const SourceDesc& rhs) {
				return lhs.type == rhs.type
					&& lhs.id == rhs.id;
			}

			friend bool operator!=(const SourceDesc& lhs, const SourceDesc& rhs) {
				return !(lhs == rhs);
			}
		};


		enum class State {
			eOK,
			eDEVICE_CONNECTION_ERROR,
			eMANUALLY_DISCONNECTED,
			eRECONNECT_NEEDED,
			eDEVICE_IS_EXCLUSIVE,
		};

		struct Snapshot {
			State state = State::eMANUALLY_DISCONNECTED;
			string name;
			string description;
			string id;
			string formatString;
			wasapi_wrappers::MediaDeviceType type{};
			wasapi_wrappers::WaveFormat format;
			string channelsString;
		};

	private:
		Logger logger;
		Version version{};
		double bufferSizeSec = 0.0;

		wasapi_wrappers::MediaDeviceEnumerator enumeratorWrapper;

		wasapi_wrappers::MediaDeviceHandle audioDeviceHandle;
		wasapi_wrappers::AudioCaptureClient audioCaptureClient;
		wasapi_wrappers::AudioSessionEventsWrapper sessionEventsWrapper;
		GenericComWrapper<IAudioRenderClient> renderClient;
		ChannelMixer channelMixer;

		Snapshot snapshot;

		index lastExclusiveProcessId = -1;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void setVersion(Version value) {
			version = value;
		}

		void setBufferSizeInSec(double value);

		void setSource(const SourceDesc& desc);
		void disconnect();

	private:
		[[nodiscard]]
		State setSourceAndGetState(const SourceDesc& desc);

	public:
		const Snapshot& getSnapshot() const {
			return snapshot;
		}

		[[nodiscard]]
		auto getState() const {
			return snapshot.state;
		}

		// returns true if at least one buffer was captured
		bool capture();

		[[nodiscard]]
		const auto& getChannelMixer() const {
			return channelMixer;
		}

		void tryToRecoverFromExclusive();

	private:
		[[nodiscard]]
		std::optional<wasapi_wrappers::MediaDeviceHandle> getDevice(const SourceDesc& desc);

		[[nodiscard]]
		static string makeFormatString(wasapi_wrappers::WaveFormat waveFormat);

		void createExclusiveStreamListener();

		std::vector<GenericComWrapper<IAudioSessionControl>> getActiveSessions() noexcept(false);
	};
}
