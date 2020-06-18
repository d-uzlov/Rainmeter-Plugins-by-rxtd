/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CaptureManager.h"

#include "windows-wrappers/WaveFormatWrapper.h"

static constexpr long long REF_TIMES_PER_SEC = 1000'000'0; // 1 sec in 100-ns units

const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioClient = __uuidof(IAudioClient);

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

namespace rxtd::audio_analyzer {
	CaptureManager::CaptureManager(utils::Rainmeter::Logger& logger, IMMDevice& audioDeviceHandle, bool loopback) : logger(&logger) {
		HRESULT hr = audioDeviceHandle.Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
			reinterpret_cast<void**>(&audioClient));
		if (hr != S_OK) {
			valid = false;
			logger.error(L"Can't create AudioClient, error code {}", hr);
			return;
		}

		utils::WaveFormatWrapper waveFormatWrapper;
		hr = audioClient->GetMixFormat(&waveFormatWrapper);
		if (hr != S_OK) {
			valid = false;
			logger.error(L"GetMixFormat() failed, error code {}", hr);
			return;
		}

		auto formatOpt = parseStreamFormat(waveFormatWrapper.getPointer());
		if (!formatOpt.has_value()) {
			valid = false;
			logger.error(L"Invalid sample format, error code {}", waveFormatWrapper.getPointer()->wFormatTag);
			return;
		}
		waveFormat = formatOpt.value();

		hr = audioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
			REF_TIMES_PER_SEC,
			0,
			waveFormatWrapper.getPointer(),
			nullptr);
		if (hr != S_OK) {
			if (hr == AUDCLNT_E_DEVICE_IN_USE) {
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
			logger.error(L"AudioClient.Initialize() fail, error code {}", hr);
			return;
		}

		hr = audioClient->GetService(IID_IAudioCaptureClient, reinterpret_cast<void**>(&audioCaptureClient));
		if (hr != S_OK) {
			valid = false;
			logger.error(L"Can't create AudioCaptureClient, error code {}", hr);
			return;
		}

		hr = audioClient->Start();
		if (hr != S_OK) {
			valid = false;
			logger.error(L"Can't start stream, error code {}", hr);
			return;
		}

		formatString = makeFormatString(waveFormat);
	}

	CaptureManager::~CaptureManager() {
		invalidate();
	}

	CaptureManager::CaptureManager(CaptureManager&& other) noexcept :
		audioClient(std::move(other.audioClient)),
		audioCaptureClient(std::move(other.audioCaptureClient)),
		waveFormat(std::move(other.waveFormat)) {
		valid = other.valid;
		other.valid = false;

		recoverable = other.recoverable;
		other.recoverable = false;
	}

	CaptureManager& CaptureManager::operator=(CaptureManager&& other) noexcept {
		if (this == &other)
			return *this;

		audioClient = std::move(other.audioClient);
		audioCaptureClient = std::move(other.audioCaptureClient);
		waveFormat = std::move(other.waveFormat);

		valid = other.valid;
		other.valid = false;

		recoverable = other.recoverable;
		other.recoverable = false;

		return *this;
	}

	MyWaveFormat CaptureManager::getWaveFormat() const {
		return waveFormat;
	}

	const string& CaptureManager::getFormatString() const {
		return formatString;
	}

	bool CaptureManager::isEmpty() const {
		return !audioCaptureClient.isValid() || !audioClient.isValid();
	}

	bool CaptureManager::isValid() const {
		// return !isEmpty() && waveFormat.format != Format::eINVALID;
		return valid;
	}

	bool CaptureManager::isRecoverable() const {
		return recoverable;
	}

	utils::BufferWrapper CaptureManager::readBuffer() {
		return utils::BufferWrapper(audioCaptureClient.getPointer());
	}

	CaptureManager::BufferFetchResult CaptureManager::nextBuffer() {
		if (!isValid()) {
			return BufferFetchResult::deviceError();
		}

		auto bufferWrapper = readBuffer();

		const auto queryResult = bufferWrapper.getResult();
		const auto now = clock::now();

		switch (queryResult) {
		case S_OK:
			lastBufferFillTime = now;

			return bufferWrapper;

		case AUDCLNT_S_BUFFER_EMPTY:
			// Windows bug: sometimes when shutting down a playback application, it doesn't zero
			// out the buffer.  Detect this by checking the time since the last successful fill
			// and resetting the volumes if past the threshold.
			if (now - lastBufferFillTime >= EMPTY_TIMEOUT) {
				return BufferFetchResult::deviceError();
			}
			return BufferFetchResult::noData();

		case AUDCLNT_E_BUFFER_ERROR:
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
			logger->debug(L"Audio device disconnected");
			invalidate();
			return BufferFetchResult::deviceError();

		default:
			logger->warning(L"Unexpected buffer query error code {error}", queryResult);
			invalidate();
			return BufferFetchResult::deviceError();
		}
	}

	void CaptureManager::invalidate() {
		audioCaptureClient = { };
		audioClient = { };
		waveFormat = { };
		formatString = { };
		valid = false;
	}

	std::optional<MyWaveFormat> CaptureManager::parseStreamFormat(WAVEFORMATEX* waveFormatEx) {
		MyWaveFormat waveFormat;
		waveFormat.channelsCount = waveFormatEx->nChannels;
		waveFormat.samplesPerSec = waveFormatEx->nSamplesPerSec;

		if (waveFormatEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			const auto formatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(waveFormatEx);

			if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && waveFormatEx->wBitsPerSample == 16) {
				waveFormat.format = Format::ePCM_S16;
			} else if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
				waveFormat.format = Format::ePCM_F32;
			} else {
				return std::nullopt;
			}

			waveFormat.channelLayout = ChannelLayouts::layoutFromChannelMask(formatExtensible->dwChannelMask, true);
			// TODO second param should be an option

			return waveFormat;
		}

		if (waveFormatEx->wFormatTag == WAVE_FORMAT_PCM && waveFormatEx->wBitsPerSample == 16) {
			waveFormat.format = Format::ePCM_S16;
		} else if (waveFormatEx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
			waveFormat.format = Format::ePCM_F32;
		} else {
			return std::nullopt;
		}

		if (waveFormatEx->nChannels == 1) {
			waveFormat.channelLayout = ChannelLayouts::getMono();
		} else if (waveFormatEx->nChannels == 2) {
			waveFormat.channelLayout = ChannelLayouts::getStereo();
		} else {
			return std::nullopt;
		}

		return waveFormat;
	}

	string CaptureManager::makeFormatString(MyWaveFormat waveFormat) {
		if (waveFormat.format == Format::eINVALID) {
			return waveFormat.format.toString();
		}

		string format;
		format.clear();

		format.reserve(64);

		format += waveFormat.format.toString();
		format += L", "sv;

		format += std::to_wstring(waveFormat.samplesPerSec);
		format += L"Hz, "sv;

		if (waveFormat.channelLayout.getName().empty()) {
			format += L"unknown layout: "sv;
			format += std::to_wstring(waveFormat.channelsCount);
			format += L"ch"sv;
		} else {
			format += waveFormat.channelLayout.getName();
		}

		return format;
	}
}
