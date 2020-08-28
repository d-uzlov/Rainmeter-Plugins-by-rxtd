/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "MyWaveFormat.h"
#include "windows-wrappers/MediaDeviceWrapper.h"
#include "RainmeterWrappers.h"
#include "windows-wrappers/IAudioCaptureClientWrapper.h"
#include <functional>

#include "AudioEnumeratorHelper.h"
#include "AudioSessionEventsWrapper.h"
#include "../ChannelMixer.h"
#include "windows-wrappers/implementations/AudioSessionEventsImpl.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
		enum class DataSource {
			eDEFAULT_INPUT,
			eDEFAULT_OUTPUT,
			eID,
		};

		enum class State {
			eOK,
			eDEVICE_CONNECTION_ERROR,
			eRECONNECT_NEEDED,
			eDEVICE_IS_EXCLUSIVE
		};

		struct Snapshot {
			State state = State::eDEVICE_CONNECTION_ERROR;
			string name;
			string nameOnly;
			string description;
			string id;
			string formatString;
			utils::MediaDeviceType type{ };
			MyWaveFormat format;
		};

	private:
		utils::Rainmeter::Logger logger;
		index legacyNumber = 0;
		index bufferSize100NsUnits{ };

		AudioEnumeratorHelper enumerator;

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

		void setSource(DataSource type, const string& id);
		void disconnect();

	private:
		[[nodiscard]]
		State setSourceAndGetState(DataSource type, const string& id);

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
		std::optional<utils::MediaDeviceWrapper> getDevice(DataSource type, const string& id);

		[[nodiscard]]
		static string makeFormatString(MyWaveFormat waveFormat);

		void createExclusiveStreamListener();

		std::vector<utils::GenericComWrapper<IAudioSessionControl>> getActiveSessions();
	};
}
