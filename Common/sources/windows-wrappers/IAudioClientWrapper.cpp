/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IAudioClientWrapper.h"

#include "BufferPrinter.h"

using namespace utils;

IAudioClientWrapper::IAudioClientWrapper(InitFunction initFunction) :
	GenericComWrapper(std::move(initFunction)) {
}

IAudioCaptureClientWrapper IAudioClientWrapper::openCapture() {
	auto result = IAudioCaptureClientWrapper{
		[&](auto ptr) {
			lastResult = getPointer()->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(ptr));
			return lastResult == S_OK;
		}
	};

	result.setParams(formatType, format.channelsCount);

	return result;
}

void IAudioClientWrapper::testExclusive() {
	WAVEFORMATEX* waveFormat;
	lastResult = getPointer()->GetMixFormat(&waveFormat);
	if (lastResult != S_OK) {
		return;
	}

	auto client3 = utils::GenericComWrapper<IAudioClient3>{
		[&](auto ptr) {
			lastResult = getPointer()->QueryInterface<IAudioClient3>(ptr);
			return S_OK == lastResult;
		}
	};

	if (!client3.isValid()) {
		// Both IAudioClient#Initialize and IAudioClient3#InitializeSharedAudioStream leak commit memory
		// but according to me tests,
		// if provided with minimum buffer size
		// IAudioClient3#InitializeSharedAudioStream leaks less
		// but if IAudioClient3 is not available,
		//	which, by the way, is probably caused by old windows version
		// then IAudioClient#Initialize also fits the task
		//
		// It doesn't matter if we need AUDCLNT_STREAMFLAGS_LOOPBACK for this session
		// exclusive streams don't allow shared connection both with and without loopback
		lastResult = getPointer()->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			0,
			0,
			waveFormat,
			nullptr
		);
		return;
	}

	UINT32 pDefaultPeriodInFrames;
	UINT32 pFundamentalPeriodInFrames;
	UINT32 pMinPeriodInFrames;
	UINT32 pMaxPeriodInFrames;
	lastResult = client3.getPointer()->GetSharedModeEnginePeriod(
		waveFormat,
		&pDefaultPeriodInFrames,
		&pFundamentalPeriodInFrames,
		&pMinPeriodInFrames,
		&pMaxPeriodInFrames
	);
	if (lastResult != S_OK) {
		return;
	}

	lastResult = client3.getPointer()->InitializeSharedAudioStream(
		0,
		pMinPeriodInFrames,
		waveFormat,
		nullptr
	);
	CoTaskMemFree(waveFormat);
}

void IAudioClientWrapper::initShared(index bufferSize100nsUnits) {
	WAVEFORMATEX* waveFormat;
	lastResult = getPointer()->GetMixFormat(&waveFormat);
	switch (lastResult) {
	case AUDCLNT_E_DEVICE_INVALIDATED:
	case AUDCLNT_E_SERVICE_NOT_RUNNING:
	case E_OUTOFMEMORY:
		return;
	default: ; // todo
	}

	// Documentation for IAudioClient::Initialize says
	// ""
	// Note  In Windows 8, the first use of IAudioClient to access the audio device
	// should be on the STA thread. Calls from an MTA thread may result in undefined behavior.
	// ""
	// Source: https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
	// However, there is also a different information:
	// ""
	// The MSDN docs for ActivateAudioInterfaceAsync say that <...>
	// They also say that "In Windows 8, the first use of IAudioClient to access the audio device should be on the STA thread <...>"
	// That's incorrect. The first use of IAudioClient.Initialize must be inside your
	// IActivateAudioInterfaceCompletionHandler.ActivateCompleted handler,
	// which will have been invoked on a background thread by ActivateAudioInterfaceAsync.
	// ""
	// Source: https://www.codeproject.com/Articles/460145/Recording-and-playing-PCM-audio-on-Windows-8-VB
	// In this citation they speak about ActivateAudioInterfaceAsync() function
	// but I believe that if that is true for that function
	// then it should be true for all calls to IAudioClient#Initialize
	//
	// So I don't do anything to specifically call this function from STA
	lastResult = getPointer()->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		bufferSize100nsUnits,
		0,
		waveFormat,
		nullptr
	);
	if (lastResult == AUDCLNT_E_WRONG_ENDPOINT_TYPE) {
		lastResult = getPointer()->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			bufferSize100nsUnits,
			0,
			waveFormat,
			nullptr
		);
		type = MediaDeviceType::eINPUT;
	} else {
		type = MediaDeviceType::eOUTPUT;
	}

	if (lastResult != S_OK) {
		CoTaskMemFree(waveFormat);
		return;
	}

	formatIsValid = true;

	format.channelsCount = waveFormat->nChannels;
	format.samplesPerSec = waveFormat->nSamplesPerSec;

	if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		const auto formatExtensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(waveFormat);

		if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && waveFormat->wBitsPerSample == 16) {
			formatType = IAudioCaptureClientWrapper::Type::eInt16;
		} else if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			formatType = IAudioCaptureClientWrapper::Type::eFloat;
		} else {
			formatIsValid = false;
		}

		format.channelMask = formatExtensible->dwChannelMask;
	} else {
		if (waveFormat->wFormatTag == WAVE_FORMAT_PCM && waveFormat->wBitsPerSample == 16) {
			formatType = IAudioCaptureClientWrapper::Type::eInt16;
		} else if (waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
			formatType = IAudioCaptureClientWrapper::Type::eFloat;
		} else {
			formatIsValid = false;
		}

		if (waveFormat->nChannels == 1) {
			format.channelMask = KSAUDIO_SPEAKER_MONO;
		} else if (waveFormat->nChannels == 2) {
			format.channelMask = KSAUDIO_SPEAKER_STEREO;
		} else {
			formatIsValid = false;
		}
	}

	CoTaskMemFree(waveFormat);
}
