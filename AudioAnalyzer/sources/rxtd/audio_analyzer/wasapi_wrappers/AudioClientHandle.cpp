/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioClientHandle.h"

#include "rxtd/std_fixes/MyMath.h"
#include "rxtd/winapi_wrappers/ComException.h"
#include "rxtd/winapi_wrappers/GenericCoTaskMemWrapper.h"

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::wasapi_wrappers::AudioClientHandle;
using rxtd::audio_analyzer::wasapi_wrappers::AudioCaptureClient;

AudioCaptureClient AudioClientHandle::openCapture(double bufferSizeSec) {
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

	uint32_t flags = 0;
	if (type == MediaDeviceType::eOUTPUT) {
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
	}

	throwOnError(
		ref().Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			flags,
			bufferSize100nsUnits,
			0,
			nativeFormat.getPointer(),
			nullptr
		),
		L"IAudioClient.Initialize() in IAudioClientWrapper::openCapture()"
	);

	auto result = AudioCaptureClient{
		[&](auto ptr) {
			typedQuery(&IAudioClient::GetService, ptr, L"IAudioClient.GetService(IAudioCaptureClient) in IAudioClientWrapper::openCapture()");
		}
	};

	result.setParams(formatType, format.channelsCount);

	return result;
}

rxtd::winapi_wrappers::GenericComWrapper<IAudioRenderClient> AudioClientHandle::openRender() noexcept(false) {
	throwOnError(
		ref().Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			0,
			0,
			nativeFormat.getPointer(),
			nullptr
		),
		L"IAudioClient.Initialize() in IAudioClientWrapper::openRender()"
	);

	auto result = GenericComWrapper<IAudioRenderClient>{
		[&](auto ptr) {
			typedQuery(&IAudioClient::GetService, ptr, L"IAudioClient.GetService(IAudioCaptureClient) in IAudioClientWrapper::openRender()");
		}
	};

	return result;
}

void AudioClientHandle::testExclusive() {
	auto client3 = GenericComWrapper<IAudioClient3>{
		[&](auto ptr) {
			auto code = ref().QueryInterface<IAudioClient3>(ptr);
			return S_OK == code;
		}
	};

	// Both IAudioClient#Initialize and IAudioClient3#InitializeSharedAudioStream leak commit memory
	// when device is in exclusive mode (AUDCLNT_E_DEVICE_IN_USE return code)
	// but according to my tests, [when provided with minimum buffer size]
	// IAudioClient3#InitializeSharedAudioStream leaks less, so this is the preferable method of testing

	// Looks like InitializeSharedAudioStream adds crackling to sound in some cases.
	// Unfortunately, it's not consistent, so it's impossible to know for sure.
	// If after disabling this if-branch issue won't be present for long time,
	// then I would assume that InitializeSharedAudioStream is really the cause of this,
	// and remove this disabled code.
	if (client3.isValid() && false) {
		UINT32 pDefaultPeriodInFrames;
		UINT32 pFundamentalPeriodInFrames;
		UINT32 pMinPeriodInFrames;
		UINT32 pMaxPeriodInFrames;
		throwOnError(
			client3.ref().GetSharedModeEnginePeriod(
				nativeFormat.getPointer(),
				&pDefaultPeriodInFrames,
				&pFundamentalPeriodInFrames,
				&pMinPeriodInFrames,
				&pMaxPeriodInFrames
			), L"IAudioClient3.GetSharedModeEnginePeriod() in IAudioClientWrapper::testExclusive()"
		);

		throwOnError(
			client3.ref().InitializeSharedAudioStream(
				0,
				pMinPeriodInFrames,
				nativeFormat.getPointer(),
				nullptr
			), L"IAudioClient3.InitializeSharedAudioStream() in IAudioClientWrapper::testExclusive()"
		);
	} else {
		throwOnError(
			ref().Initialize(
				AUDCLNT_SHAREMODE_SHARED,
				AUDCLNT_STREAMFLAGS_LOOPBACK,
				0,
				0,
				nativeFormat.getPointer(),
				nullptr
			), L"IAudioClient.Initialize() in IAudioClientWrapper::testExclusive()"
		);
	}
}

void AudioClientHandle::readFormat() {
	nativeFormat = {
		[&](auto ptr) {
			throwOnError(ref().GetMixFormat(ptr), L"WAVEFORMATEX.GetMixFormat() in IAudioClientWrapper::()");
			return true;
		}
	};

	auto& waveFormatRaw = *nativeFormat.getPointer();
	format.channelsCount = waveFormatRaw.nChannels;
	format.samplesPerSec = waveFormatRaw.nSamplesPerSec;

	uint32_t channelMask{};

	if (waveFormatRaw.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		const auto& formatExtensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(waveFormatRaw);

		if (formatExtensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM && formatExtensible.Format.wBitsPerSample == 16) {
			formatType = AudioCaptureClient::Type::eInt16;
		} else if (formatExtensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			formatType = AudioCaptureClient::Type::eFloat;
		} else {
			throw FormatException{};
		}

		channelMask = formatExtensible.dwChannelMask;
	} else {
		if (waveFormatRaw.wFormatTag == WAVE_FORMAT_PCM && waveFormatRaw.wBitsPerSample == 16) {
			formatType = AudioCaptureClient::Type::eInt16;
		} else if (waveFormatRaw.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
			formatType = AudioCaptureClient::Type::eFloat;
		} else {
			throw FormatException{};
		}

		if (waveFormatRaw.nChannels == 1) {
			channelMask = KSAUDIO_SPEAKER_MONO;
		} else if (waveFormatRaw.nChannels == 2) {
			channelMask = KSAUDIO_SPEAKER_STEREO;
		} else {
			throw FormatException{};
		}
	}

	format.channelLayout = audio_analyzer::ChannelUtils::parseLayout(channelMask, true);
}
