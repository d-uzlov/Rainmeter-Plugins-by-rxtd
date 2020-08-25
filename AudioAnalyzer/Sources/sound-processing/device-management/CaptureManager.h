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
#include <chrono>
#include "windows-wrappers/IAudioCaptureClientWrapper.h"
#include <functional>

#include "AudioEnumeratorHelper.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
		using ProcessingCallback = std::function<void(utils::array2d_view<float> channels)>;

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
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		utils::Rainmeter::Logger logger;
		index legacyNumber = 0;

		AudioEnumeratorHelper enumerator;

		utils::IAudioCaptureClientWrapper audioCaptureClient;

		clock::time_point lastBufferFillTime{ };
		static constexpr clock::duration EMPTY_TIMEOUT = std::chrono::milliseconds(100);

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

		void capture(const ProcessingCallback& processingCallback);

	private:
		void invalidate();

		[[nodiscard]]
		std::optional<utils::MediaDeviceWrapper> getDevice(DataSource type, const string& id);

		[[nodiscard]]
		static string makeFormatString(MyWaveFormat waveFormat);
	};
}
