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
#include "../ChannelMixer.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
		enum class DataSource {
			eDEFAULT_INPUT,
			eDEFAULT_OUTPUT,
			eID,
		};

		struct Snapshot {
			bool status{ };
			string name;
			string description;
			string id;
			string formatString;
			utils::MediaDeviceType type{ };
			MyWaveFormat format;
		};

	private:
		utils::Rainmeter::Logger logger;
		index legacyNumber = 0;

		AudioEnumeratorHelper enumerator;

		utils::IAudioCaptureClientWrapper audioCaptureClient;
		ChannelMixer channelMixer;

		bool valid = true;

		Snapshot snapshot;

	public:
		CaptureManager() = default;

		~CaptureManager() {
			invalidate();
		}

		CaptureManager(CaptureManager&& other) noexcept = default;
		CaptureManager& operator=(CaptureManager&& other) noexcept = default;

		CaptureManager(const CaptureManager& other) = delete;
		CaptureManager& operator=(const CaptureManager& other) = delete;


		void setLogger(utils::Rainmeter::Logger value) {
			logger = std::move(value);
		}

		void setLegacyNumber(index value) {
			legacyNumber = value;
		}

		// returns false on fatal error, true otherwise
		[[nodiscard]]
		bool setSource(DataSource type, const string& id);

		void updateSnapshot(Snapshot& snap) const {
			snap = snapshot;
		}

		[[nodiscard]]
		bool isValid() const {
			return valid;
		}

		// returns true is at least one buffer was captured
		bool capture();

		const auto& getChannelMixer() const {
			return channelMixer;
		}

	private:
		void invalidate();

		[[nodiscard]]
		std::optional<utils::MediaDeviceWrapper> getDevice(DataSource type, const string& id);

		[[nodiscard]]
		static string makeFormatString(MyWaveFormat waveFormat);
	};
}
