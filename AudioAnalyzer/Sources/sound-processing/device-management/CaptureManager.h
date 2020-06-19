/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "windows-wrappers/AudioBuffer.h"
#include "MyWaveFormat.h"
#include "windows-wrappers/GenericComWrapper.h"
#include "windows-wrappers/MediaDeviceWrapper.h"
#include "RainmeterWrappers.h"
#include <chrono>
#include "windows-wrappers/IAudioCaptureClientWrapper.h"

namespace rxtd::audio_analyzer {
	class CaptureManager {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

	public:
		enum class BufferFetchState {
			eOK,
			eNO_DATA,
			eDEVICE_ERROR,
			eINVALID_STATE,
		};

		class BufferFetchResult {
			const BufferFetchState state;
			const utils::AudioBuffer buffer;

			explicit BufferFetchResult(BufferFetchState state)
				: state(state), buffer(utils::AudioBuffer()) { }

		public:
			static BufferFetchResult noData() {
				return BufferFetchResult(BufferFetchState::eNO_DATA);
			}
			static BufferFetchResult deviceError() {
				return BufferFetchResult(BufferFetchState::eDEVICE_ERROR);
			}
			static BufferFetchResult invalidState() {
				return BufferFetchResult(BufferFetchState::eINVALID_STATE);
			}

			BufferFetchResult(utils::AudioBuffer&& buffer)
				: state(BufferFetchState::eOK), buffer(buffer) { }

			BufferFetchState getState() const {
				return state;
			}
			const utils::AudioBuffer& getBuffer() const {
				return buffer;
			}
		};

	private:
		utils::Rainmeter::Logger *logger = nullptr;

		utils::IAudioClientWrapper audioClient { };
		utils::IAudioCaptureClientWrapper audioCaptureClient { };
		MyWaveFormat waveFormat { };
		string formatString { };

		clock::time_point lastBufferFillTime { };
		static constexpr clock::duration EMPTY_TIMEOUT = std::chrono::milliseconds(100);

		bool valid = true;
		bool recoverable = true;

	public:
		CaptureManager() = default;
		CaptureManager(utils::Rainmeter::Logger& logger, utils::MediaDeviceWrapper& audioDeviceHandle, bool loopback);
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
		BufferFetchResult nextBuffer();

	private:
		void invalidate();

		static string makeFormatString(MyWaveFormat waveFormat);
	};
}
