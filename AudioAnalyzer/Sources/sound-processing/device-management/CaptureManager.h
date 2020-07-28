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
#include "array_view.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
	public:
		using ProcessingCallback = std::function<void(bool silent, array_view<std::byte> frameBuffer)>;

	private:
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		utils::Rainmeter::Logger logger;

		utils::IAudioClientWrapper audioClient{ };
		utils::IAudioCaptureClientWrapper audioCaptureClient{ };
		MyWaveFormat waveFormat{ };
		string formatString{ };

		clock::time_point lastBufferFillTime{ };
		static constexpr clock::duration EMPTY_TIMEOUT = std::chrono::milliseconds(100);

		bool valid = true;
		bool recoverable = true;

	public:
		CaptureManager() = default;
		CaptureManager(utils::Rainmeter::Logger logger, utils::MediaDeviceWrapper& audioDeviceHandle);
		~CaptureManager();

		CaptureManager(CaptureManager&& other) noexcept = default;
		CaptureManager& operator=(CaptureManager&& other) noexcept = default;

		CaptureManager(const CaptureManager& other) = delete;
		CaptureManager& operator=(const CaptureManager& other) = delete;

		MyWaveFormat getWaveFormat() const;
		const string& getFormatString() const;
		bool isEmpty() const;
		bool isValid() const;
		bool isRecoverable() const;
		void capture(const ProcessingCallback& processingCallback);

	private:
		void invalidate();

		static string makeFormatString(MyWaveFormat waveFormat);
	};
}
