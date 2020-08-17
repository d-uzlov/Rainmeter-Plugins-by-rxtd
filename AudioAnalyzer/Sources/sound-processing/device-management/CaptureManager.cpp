/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CaptureManager.h"

namespace rxtd::audio_analyzer {
	CaptureManager::CaptureManager(
		utils::Rainmeter::Logger _logger, utils::MediaDeviceWrapper& audioDeviceHandle
	) : logger(std::move(_logger)) {
		audioClient = audioDeviceHandle.openAudioClient();
		if (audioDeviceHandle.getLastResult() != S_OK) {
			valid = false;
			logger.error(L"Can't create AudioClient, error code {}", audioDeviceHandle.getLastResult());
			return;
		}

		audioClient.initShared();
		if (audioClient.getLastResult() != S_OK) {
			if (audioClient.getLastResult() == AUDCLNT_E_DEVICE_IN_USE) {
				// If device is in exclusive mode, then call to Initialize() above leads to leak in Commit memory area
				// Tested on LTSB 1607, last updates as of 2019-01-10
				// Google "WASAPI exclusive memory leak"
				// I consider this error unrecoverable to prevent further leaks
				valid = false;
				recoverable = false;
				logger.error(L"Device operates in exclusive mode, won't recover");
				return;
			}
			valid = false;
			logger.error(L"AudioClient.Initialize() fail, error code {}", audioClient.getLastResult());
			return;
		}

		const auto format = audioClient.getFormat();
		waveFormat.channelsCount = format.channelsCount;
		waveFormat.samplesPerSec = format.samplesPerSec;
		waveFormat.channelLayout = ChannelLayouts::layoutFromChannelMask(uint32_t(format.channelMask), true);
		waveFormat.format = format.format;
		if (waveFormat.format == utils::WaveDataFormat::eINVALID) {
			logger.error(L"Invalid sample format");
			valid = false;
			return;
		}

		audioCaptureClient = audioClient.openCapture();
		if (audioClient.getLastResult() != S_OK) {
			valid = false;
			logger.error(L"Can't create AudioCaptureClient, error code {}", audioClient.getLastResult());
			return;
		}

		HRESULT hr = audioClient.getPointer()->Start();
		if (hr != S_OK) {
			valid = false;
			logger.error(L"Can't start stream, error code {}", hr);
			return;
		}

		formatString = makeFormatString(waveFormat);

		lastBufferFillTime = clock::now();
	}

	CaptureManager::~CaptureManager() {
		invalidate();
	}

	MyWaveFormat CaptureManager::getWaveFormat() const {
		return waveFormat;
	}

	const string& CaptureManager::getFormatString() const {
		return formatString;
	}

	bool CaptureManager::isEmpty() const {
		// TODO unused
		return !audioCaptureClient.isValid() || !audioClient.isValid();
	}

	bool CaptureManager::isValid() const {
		// return !isEmpty() && waveFormat.format != Format::eINVALID;
		return valid;
	}

	bool CaptureManager::isRecoverable() const {
		return recoverable;
	}

	void CaptureManager::capture(const ProcessingCallback& processingCallback) {
		if (!isValid()) {
			return;
		}

		while (true) {
			audioCaptureClient.readBuffer();

			const auto queryResult = audioCaptureClient.getLastResult();
			const auto now = clock::now();

			switch (queryResult) {
			case S_OK:
				lastBufferFillTime = now;

				processingCallback(audioCaptureClient.getBuffer());
				break;

			case AUDCLNT_S_BUFFER_EMPTY:
				// Windows bug: sometimes when shutting down a playback application, it doesn't zero out the buffer.
				// rxtd: I don't really understand this. I can't reproduce this and I don't know if this workaround do anything useful
				if (now - lastBufferFillTime >= EMPTY_TIMEOUT) {
					const std::chrono::duration<float, std::milli> overheadTime = now - lastBufferFillTime;
					logger.debug(L"poll timeout: {} ms", overheadTime.count());
					invalidate();
				}
				return;

			case AUDCLNT_E_BUFFER_ERROR:
			case AUDCLNT_E_DEVICE_INVALIDATED:
			case AUDCLNT_E_SERVICE_NOT_RUNNING:
				invalidate();
				return;

			default:
				logger.warning(L"Unexpected buffer query error code {error}", queryResult);
				invalidate();
				return;
			}
		}
	}

	void CaptureManager::invalidate() {
		audioCaptureClient = { };
		audioClient = { };
		waveFormat = { };
		formatString = { };
		valid = false;
	}

	string CaptureManager::makeFormatString(MyWaveFormat waveFormat) {
		using Format = utils::WaveDataFormat;

		if (waveFormat.format == Format::eINVALID) {
			return L"<invalid>";
		}

		// todo use buffer printer
		string format;
		format.clear();

		format.reserve(64);

		switch (waveFormat.format) {
		case Format::ePCM_S16:
			format += L"PCM 16b";
			break;
		case Format::ePCM_F32:
			format += L"PCM 32b";
			break;
		case Format::eINVALID: ;
		default: std::terminate();
		}

		format += L", ";

		format += std::to_wstring(waveFormat.samplesPerSec);
		format += L"Hz, ";

		if (waveFormat.channelLayout.getName().empty()) {
			format += L"unknown layout: ";
			format += std::to_wstring(waveFormat.channelsCount);
			format += L"ch";
		} else {
			format += waveFormat.channelLayout.getName();
		}

		return format;
	}
}
