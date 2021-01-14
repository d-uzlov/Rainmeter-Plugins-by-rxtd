/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IAudioClientWrapper.h"

#include "BufferPrinter.h"
#include "GenericCoTaskMemWrapper.h"
#include "MyMath.h"

using namespace utils;

IAudioCaptureClientWrapper IAudioClientWrapper::openCapture() {
	auto result = IAudioCaptureClientWrapper{
		[&](auto ptr) {
			lastResult = typedQuery(&IAudioClient::GetService, ptr);
			return lastResult == S_OK;
		}
	};

	result.setParams(formatType, format.channelsCount);

	return result;
}

void IAudioClientWrapper::testExclusive() {
	readFormat();
	if (!formatIsValid) {
		return;
	}

	auto client3 = utils::GenericComWrapper<IAudioClient3>{
		[&](auto ptr) {
			lastResult = ref().QueryInterface<IAudioClient3>(ptr);
			return S_OK == lastResult;
		}
	};

	if (!client3.isValid()) {
		// Both IAudioClient#Initialize and IAudioClient3#InitializeSharedAudioStream leak commit memory
		// but according to my tests,
		// if provided with minimum buffer size
		// IAudioClient3#InitializeSharedAudioStream leaks less
		// but if IAudioClient3 is not available,
		//	which, by the way, is probably caused by old windows version
		// then IAudioClient#Initialize also fits the task
		//
		// It doesn't matter if we need AUDCLNT_STREAMFLAGS_LOOPBACK for this session
		// exclusive streams don't allow shared connection both with and without loopback
		lastResult = ref().Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			0,
			0,
			nativeFormat.getPointer(),
			nullptr
		);
		return;
	}

	UINT32 pDefaultPeriodInFrames;
	UINT32 pFundamentalPeriodInFrames;
	UINT32 pMinPeriodInFrames;
	UINT32 pMaxPeriodInFrames;
	lastResult = client3.ref().GetSharedModeEnginePeriod(
		nativeFormat.getPointer(),
		&pDefaultPeriodInFrames,
		&pFundamentalPeriodInFrames,
		&pMinPeriodInFrames,
		&pMaxPeriodInFrames
	);
	if (lastResult != S_OK) {
		return;
	}

	lastResult = client3.ref().InitializeSharedAudioStream(
		0,
		pMinPeriodInFrames,
		nativeFormat.getPointer(),
		nullptr
	);
}

void IAudioClientWrapper::initShared(double bufferSizeSec) {
	readFormat();
	if (!formatIsValid) {
		return;
	}

	// Documentation for IAudioClient::Initialize says
	// ""
	//	Note  In Windows 8, the first use of IAudioClient to access the audio device
	//	should be on the STA thread. Calls from an MTA thread may result in undefined behavior.
	// ""
	// Source: https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
	//
	// However, there is also a different information:
	// ""
	//	The MSDN docs for ActivateAudioInterfaceAsync say that <...>
	//	They also say that "In Windows 8, the first use of IAudioClient to access the audio device should be on the STA thread <...>"
	//	That's incorrect. The first use of IAudioClient.Initialize must be inside your
	//	IActivateAudioInterfaceCompletionHandler.ActivateCompleted handler,
	//	which will have been invoked on a background thread by ActivateAudioInterfaceAsync.
	// ""
	// Source: https://www.codeproject.com/Articles/460145/Recording-and-playing-PCM-audio-on-Windows-8-VB
	//
	// In this citation they speak about ActivateAudioInterfaceAsync() function
	// but I believe that if that's true for that function
	// then it should also be true for all calls to IAudioClient#Initialize
	//
	// So I don't do anything to specifically call this function from STA

	constexpr double _100nsUnitsIn1sec = 1000'000'0;
	const index bufferSize100nsUnits = MyMath::roundTo<index>(bufferSizeSec * _100nsUnitsIn1sec);

	lastResult = ref().Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		bufferSize100nsUnits,
		0,
		nativeFormat.getPointer(),
		nullptr
	);
	if (lastResult == AUDCLNT_E_WRONG_ENDPOINT_TYPE) {
		lastResult = ref().Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			bufferSize100nsUnits,
			0,
			nativeFormat.getPointer(),
			nullptr
		);
		type = MediaDeviceType::eINPUT;
	} else {
		type = MediaDeviceType::eOUTPUT;
	}
}

void IAudioClientWrapper::readFormat() {
	nativeFormat = {
		[&](auto ptr) {
			lastResult = ref().GetMixFormat(ptr);
			return lastResult == S_OK;
		}
	};
	if (!nativeFormat.isValid()) {
		formatIsValid = false;
		return;
	}

	formatIsValid = true;

	auto& waveFormatRaw = *nativeFormat.getPointer();
	format.channelsCount = waveFormatRaw.nChannels;
	format.samplesPerSec = waveFormatRaw.nSamplesPerSec;

	if (waveFormatRaw.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		const auto& formatExtensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(waveFormatRaw);

		if (formatExtensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM && formatExtensible.Format.wBitsPerSample == 16) {
			formatType = IAudioCaptureClientWrapper::Type::eInt16;
		} else if (formatExtensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			formatType = IAudioCaptureClientWrapper::Type::eFloat;
		} else {
			formatIsValid = false;
		}

		format.channelMask = formatExtensible.dwChannelMask;
	} else {
		if (waveFormatRaw.wFormatTag == WAVE_FORMAT_PCM && waveFormatRaw.wBitsPerSample == 16) {
			formatType = IAudioCaptureClientWrapper::Type::eInt16;
		} else if (waveFormatRaw.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
			formatType = IAudioCaptureClientWrapper::Type::eFloat;
		} else {
			formatIsValid = false;
		}

		if (waveFormatRaw.nChannels == 1) {
			format.channelMask = KSAUDIO_SPEAKER_MONO;
		} else if (waveFormatRaw.nChannels == 2) {
			format.channelMask = KSAUDIO_SPEAKER_STEREO;
		} else {
			formatIsValid = false;
		}
	}
}
