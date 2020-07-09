/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IAudioClientWrapper.h"

static const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

static constexpr long long REF_TIMES_PER_SEC = 1000'000'0; // 1 sec in 100-ns units

namespace rxtd::utils {
	IAudioClientWrapper::IAudioClientWrapper(MediaDeviceType type, InitFunction initFunction) : 
		GenericComWrapper(std::move(initFunction)),
		type(type) {
	}

	IAudioCaptureClientWrapper IAudioClientWrapper::openCapture() {
		return IAudioCaptureClientWrapper {
			[&](auto ptr) {
				lastResult = (*this)->GetService(IID_IAudioCaptureClient, reinterpret_cast<void**>(ptr));
				return lastResult == S_OK;
			}
		};
	}

	WaveFormat IAudioClientWrapper::getFormat() const {
		return format;
	}

	void IAudioClientWrapper::initShared() {
		WAVEFORMATEX* waveFormat;
		lastResult = (*this)->GetMixFormat(&waveFormat);
		switch (lastResult) {
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
		case E_OUTOFMEMORY:
			return;
		default:;
		}

		const bool loopback = type == MediaDeviceType::eOUTPUT;
		lastResult = (*this)->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
			REF_TIMES_PER_SEC,
			0,
			waveFormat,
			nullptr);

		format.channelsCount = waveFormat->nChannels;
		format.samplesPerSec = waveFormat->nSamplesPerSec;

		if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			const auto formatExtensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(waveFormat);

			if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && waveFormat->wBitsPerSample == 16) {
				format.format = WaveDataFormat::ePCM_S16;
			} else if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
				format.format = WaveDataFormat::ePCM_F32;
			} else {
				format.format = WaveDataFormat::eINVALID;
			}

			format.channelMask = formatExtensible->dwChannelMask;
		} else {
			if (waveFormat->wFormatTag == WAVE_FORMAT_PCM && waveFormat->wBitsPerSample == 16) {
				format.format = WaveDataFormat::ePCM_S16;
			} else if (waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
				format.format = WaveDataFormat::ePCM_F32;
			} else {
				format.format = WaveDataFormat::eINVALID;
			}

			if (waveFormat->nChannels == 1) {
				format.channelMask = KSAUDIO_SPEAKER_MONO;
			} else if (waveFormat->nChannels == 2) {
				format.channelMask = KSAUDIO_SPEAKER_STEREO;
			} else {
				format.format = WaveDataFormat::eINVALID;
			}
		}

		CoTaskMemFree(waveFormat);
	}

	index IAudioClientWrapper::getLastResult() const {
		return lastResult;
	}
}

