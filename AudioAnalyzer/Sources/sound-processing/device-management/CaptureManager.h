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

#include "AudioSessionEventsWrapper.h"
#include "MyWaveFormat.h"
#include "RainmeterWrappers.h"
#include "../ChannelMixer.h"
#include "wasapi-wrappers/IAudioCaptureClientWrapper.h"
#include "wasapi-wrappers/IMMDeviceEnumeratorWrapper.h"
#include "wasapi-wrappers/MediaDeviceWrapper.h"
#include "wasapi-wrappers/implementations/AudioSessionEventsImpl.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
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
			string nameOnly;
			string description;
			string id;
			string formatString;
			utils::MediaDeviceType type{};
			MyWaveFormat format;
			string channelsString;
		};

	private:
		utils::Rainmeter::Logger logger;
		index legacyNumber = 0;
		double bufferSizeSec = 0.0;

		utils::IMMDeviceEnumeratorWrapper enumeratorWrapper;

		utils::MediaDeviceWrapper audioDeviceHandle;
		utils::IAudioCaptureClientWrapper audioCaptureClient;
		AudioSessionEventsWrapper sessionEventsWrapper;
		ChannelMixer channelMixer;

		Snapshot snapshot;

		index lastExclusiveProcessId = -1;

	public:
		void setLogger(utils::Rainmeter::Logger value) {
			logger = std::move(value);
		}

		void setLegacyNumber(index value) {
			legacyNumber = value;
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
		utils::MediaDeviceWrapper getDevice(const SourceDesc& desc);

		[[nodiscard]]
		static string makeFormatString(MyWaveFormat waveFormat);

		void createExclusiveStreamListener();

		std::vector<utils::GenericComWrapper<IAudioSessionControl>> getActiveSessions();
	};
}
