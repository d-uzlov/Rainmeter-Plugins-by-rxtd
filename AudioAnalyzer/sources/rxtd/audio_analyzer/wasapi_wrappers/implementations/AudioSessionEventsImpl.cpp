/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioSessionEventsImpl.h"


using rxtd::audio_analyzer::wasapi_wrappers::implementations::AudioSessionEventsImpl;

// {09C1EA78-063A-4026-8560-DB07EF705CFB}
const GUID AudioSessionEventsImpl::guid = { 0x9c1ea78, 0x63a, 0x4026, { 0x85, 0x60, 0xdb, 0x7, 0xef, 0x70, 0x5c, 0xfb } };;


AudioSessionEventsImpl* AudioSessionEventsImpl::create(
	AudioClientHandle& audioClient, bool preventVolumeChange
) noexcept(false) {
	AudioSessionEventsImpl& result = *new AudioSessionEventsImpl;
	result.preventVolumeChange = preventVolumeChange;

	result.sessionController = audioClient.getInterface<IAudioSessionControl>();

	if (result.sessionController.isValid()) {
		result.sessionController.throwOnError(
			result.sessionController.ref().RegisterAudioSessionNotification(&result),
			L"IAudioSessionControl.RegisterAudioSessionNotification() in AudioSessionEventsImpl factory"
		);

		result.mainVolumeController = audioClient.getInterface<ISimpleAudioVolume>();
		result.mainVolumeControllerIsValid.exchange(result.mainVolumeController.isValid());

		result.channelVolumeController = audioClient.getInterface<IChannelAudioVolume>();
		result.channelVolumeControllerIsValid.exchange(result.channelVolumeController.isValid());
	}

	result.initVolume();

	return &result;
}

AudioSessionEventsImpl* AudioSessionEventsImpl::create(GenericComWrapper<IAudioSessionControl>&& _sessionController, bool preventVolumeChange) noexcept(false) {
	AudioSessionEventsImpl& result = *new AudioSessionEventsImpl;
	result.preventVolumeChange = preventVolumeChange;

	result.sessionController = std::move(_sessionController);
	result.sessionController.throwOnError(
		result.sessionController.ref().RegisterAudioSessionNotification(&result),
		L"IAudioSessionControl.RegisterAudioSessionNotification() in AudioSessionEventsImpl factory"
	);

	result.initVolume();

	return &result;
}

void AudioSessionEventsImpl::deinit() {
	std::lock_guard<std::mutex> lock{ mut };

	if (sessionController.isValid()) {
		sessionController.ref().UnregisterAudioSessionNotification(this);
		sessionController = {};
	}

	mainVolumeController = {};
	mainVolumeControllerIsValid.exchange(mainVolumeController.isValid());

	channelVolumeController = {};
	channelVolumeControllerIsValid.exchange(mainVolumeController.isValid());
}

void AudioSessionEventsImpl::initVolume() {
	if (!preventVolumeChange) {
		return;
	}

	if (mainVolumeController.isValid()) {
		mainVolumeController.throwOnError(mainVolumeController.ref().SetMasterVolume(1.0, &guid), L"SetMasterVolume in AudioSessionEventsImpl::initVolume");
		mainVolumeController.throwOnError(mainVolumeController.ref().SetMute(false, &guid), L"SetMute in AudioSessionEventsImpl::initVolume");
	}

	if (channelVolumeController.isValid()) {
		UINT32 channelCount;
		channelVolumeController.throwOnError(channelVolumeController.ref().GetChannelCount(&channelCount), L"GetChannelCount in AudioSessionEventsImpl::initVolume");
		std::vector<float> channels(static_cast<size_t>(channelCount), 1.0f);
		channelVolumeController.throwOnError(
			channelVolumeController.ref().SetAllVolumes(channelCount, channels.data(), &guid),
			L"GetAllVolumes in AudioSessionEventsImpl::SetAllVolumes"
		);
	}
}

HRESULT AudioSessionEventsImpl::OnSimpleVolumeChanged(float volumeLevel, BOOL isMuted, const GUID* context) {
	if (!preventVolumeChange || !mainVolumeControllerIsValid.load()) {
		return S_OK;
	}

	if (context != nullptr && guid == *context) {
		return S_OK;
	}

	if (isMuted != 0) {
		const auto result = mainVolumeController.ref().SetMute(false, &guid);
		if (result != S_OK) {
			mainVolumeControllerIsValid.exchange(false);
		}
	}
	if (volumeLevel != 1.0f && mainVolumeControllerIsValid.load()) {
		const auto result = mainVolumeController.ref().SetMasterVolume(1.0f, &guid);
		if (result != S_OK) {
			mainVolumeControllerIsValid.exchange(false);
		}
	}

	return S_OK;
}

HRESULT AudioSessionEventsImpl::OnChannelVolumeChanged(DWORD channelCount, float volumes[], DWORD channel, const GUID* context) {
	if (!preventVolumeChange || !channelVolumeControllerIsValid.load()) {
		return S_OK;
	}

	if (context != nullptr && guid == *context) {
		return S_OK;
	}

	bool otherNoneOne = false;
	for (index i = 0; i < static_cast<index>(channelCount); ++i) {
		if (i != static_cast<index>(channel) && volumes[i] != 1.0f) {
			otherNoneOne = true;
			// no break because we need to set everything to 1.0
			volumes[i] = 1.0f;
		}
	}

	HRESULT res = S_OK;

	if (otherNoneOne) {
		volumes[channel] = 1.0f;
		res = channelVolumeController.ref().SetAllVolumes(
			channel, volumes, &guid
		);
	} else if (volumes[channel] != 1.0f) {
		res = channelVolumeController.ref().SetChannelVolume(
			channel, 1.0f, &guid
		);
	}

	if (res != S_OK) {
		channelVolumeControllerIsValid.exchange(false);
	}

	return S_OK;
}

HRESULT AudioSessionEventsImpl::OnSessionDisconnected(AudioSessionDisconnectReason reason) {
	if (disconnectionReason.load() != DisconnectionReason::eNONE) {
		// for some reason DisconnectReasonDeviceRemoval makes 2 calls on my PC
		return S_OK;
	}

	switch (reason) {
	case DisconnectReasonDeviceRemoval:
	case DisconnectReasonServerShutdown:
	case DisconnectReasonSessionLogoff:
	case DisconnectReasonSessionDisconnected:
		disconnectionReason.exchange(DisconnectionReason::eUNAVAILABLE);
		break;
	case DisconnectReasonFormatChanged:
		disconnectionReason.exchange(DisconnectionReason::eRECONNECT);
		break;
	case DisconnectReasonExclusiveModeOverride:
		disconnectionReason.exchange(DisconnectionReason::eEXCLUSIVE);
		break;
	}

	return S_OK;
}
