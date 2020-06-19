#include "IAudioClientWrapper.h"

const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

static constexpr long long REF_TIMES_PER_SEC = 1000'000'0; // 1 sec in 100-ns units

namespace rxtd::utils {

	IAudioCaptureClientWrapper IAudioClientWrapper::openCapture() {
		IAudioCaptureClientWrapper audioCaptureClient;
		lastResult = (*this)->GetService(IID_IAudioCaptureClient, reinterpret_cast<void**>(&audioCaptureClient));;
		return audioCaptureClient;
	}

	WaveFormat IAudioClientWrapper::getFormat() const {
		return format;
	}

	void IAudioClientWrapper::initShared(bool loopback) {
		WAVEFORMATEX* waveFormat;
		lastResult = (*this)->GetMixFormat(&waveFormat);
		switch (lastResult) {
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
		case E_OUTOFMEMORY:
			return;
		default:;
		}

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

