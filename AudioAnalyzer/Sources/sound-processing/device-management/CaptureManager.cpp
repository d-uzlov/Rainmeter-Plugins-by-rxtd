/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CaptureManager.h"

using namespace audio_analyzer;

bool CaptureManager::setSource(DataSource type, const string& id) {
	auto deviceOpt = getDevice(type, id);

	if (!deviceOpt) {
		switch (type) {
		case DataSource::eDEFAULT_INPUT:
			logger.error(L"Can't connect to default input audio device");
		case DataSource::eDEFAULT_OUTPUT:
			logger.error(L"Can't connect to default output audio device");
		case DataSource::eID:
			logger.error(L"Can't connect to audio device with id '{}'", id);
		}

		state = State::eDEVICE_CONNECTION_ERROR;
		return true;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	snapshot.status = true; // todo ?
	snapshot.id = deviceInfo.id;
	snapshot.description = deviceInfo.desc;
	snapshot.name = legacyNumber < 104 ? deviceInfo.fullFriendlyName : deviceInfo.name;
	snapshot.nameOnly = deviceInfo.name;

	auto audioClient = audioDeviceHandle.openAudioClient();
	if (audioDeviceHandle.getLastResult() != S_OK) {
		state = State::eDEVICE_CONNECTION_ERROR;
		logger.error(L"Can't create AudioClient, error code {}", audioDeviceHandle.getLastResult());
		return true;
	}

	audioClient.initShared();
	if (audioClient.getLastResult() == AUDCLNT_E_DEVICE_IN_USE) {
		state = State::eDEVICE_IS_EXCLUSIVE;
		createExclusiveStreamListener(true);
		return true;
	}

	currentExclusiveStreamId = { };

	if (audioClient.getLastResult() != S_OK) {
		state = State::eDEVICE_CONNECTION_ERROR;
		logger.error(L"AudioClient.Initialize() fail, error code {}", audioClient.getLastResult());
		return true;
	}

	const auto format = audioClient.getFormat();
	if (format.format == utils::WaveDataFormat::eINVALID) {
		logger.error(L"Invalid sample format");
		state = State::eDEVICE_CONNECTION_ERROR;
		return true;
	}

	sessionEventsWrapper.init(audioClient);

	snapshot.type = audioClient.getType();
	snapshot.format.format = format.format;
	snapshot.format.samplesPerSec = format.samplesPerSec;
	snapshot.format.channelLayout = ChannelLayouts::layoutFromChannelMask(uint32_t(format.channelMask), true);
	if (snapshot.format.channelLayout.getChannelsOrderView().empty()) {
		logger.error(L"zero known channels found in current channel layout");
		state = State::eDEVICE_CONNECTION_ERROR;
		return true;
	}

	snapshot.formatString = makeFormatString(snapshot.format);

	channelMixer.setFormat(snapshot.format);


	audioCaptureClient = audioClient.openCapture();
	if (audioClient.getLastResult() != S_OK) {
		state = State::eDEVICE_CONNECTION_ERROR;
		logger.error(L"Can't create AudioCaptureClient, error code {}", audioClient.getLastResult());
		return true;
	}

	HRESULT hr = audioClient.getPointer()->Start();
	if (hr != S_OK) {
		state = State::eDEVICE_CONNECTION_ERROR;
		logger.error(L"Can't start stream, error code {}", hr);
		return true;
	}

	state = State::eOK;
	return true;
}

bool CaptureManager::capture() {
	bool anyCaptured = false;
	channelMixer.reset();

	while (true) {
		audioCaptureClient.readBuffer();

		const auto queryResult = audioCaptureClient.getLastResult();

		if (queryResult == S_OK) {
			anyCaptured = true;
			channelMixer.saveChannelsData(audioCaptureClient.getBuffer(), true);
			continue;
		}
		if (queryResult == AUDCLNT_S_BUFFER_EMPTY) {
			break;
		}

		switch (queryResult) {
		case AUDCLNT_E_BUFFER_ERROR:
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
			break;
		default:
			logger.warning(L"Unexpected buffer query error code {error}", queryResult);
		}

		const auto changes = sessionEventsWrapper.grabChanges();
		switch (changes.disconnectionReason) {
			using DR = utils::AudioSessionEventsImpl::DisconnectionReason;
		case DR::eNONE:
			break;
		case DR::eUNAVAILABLE:
			state = State::eDEVICE_CONNECTION_ERROR;
			break;
		case DR::eRECONNECT:
			state = State::eRECONNECT_NEEDED;
			break;
		case DR::eEXCLUSIVE:
			state = State::eDEVICE_IS_EXCLUSIVE;
			createExclusiveStreamListener(true);
			break;
		}

		break;
	}

	return anyCaptured;
}

void CaptureManager::tryToRecoverFromExclusive() {
	auto audioClient = audioDeviceHandle.openAudioClient();

	auto client3 = utils::GenericComWrapper<IAudioClient3>{
		[&](auto ptr) {
			return S_OK == audioClient.getPointer()->QueryInterface<IAudioClient3>(ptr);
		}
	};

	if (!client3.isValid()) {
		return;
	}

	WAVEFORMATEX* waveFormat;
	auto res = client3.getPointer()->GetMixFormat(&waveFormat);
	if (res != S_OK) {
		return;
	}

	UINT32 pDefaultPeriodInFrames;
	UINT32 pFundamentalPeriodInFrames;
	UINT32 pMinPeriodInFrames;
	UINT32 pMaxPeriodInFrames;
	res = client3.getPointer()->GetSharedModeEnginePeriod(
		waveFormat,
		&pDefaultPeriodInFrames,
		&pFundamentalPeriodInFrames,
		&pMinPeriodInFrames,
		&pMaxPeriodInFrames
	);
	if (res != S_OK) {
		return;
	}

	res = client3.getPointer()->InitializeSharedAudioStream(
		0,
		pDefaultPeriodInFrames,
		waveFormat,
		nullptr
	);
	CoTaskMemFree(waveFormat);
	if (res == AUDCLNT_E_DEVICE_IN_USE) {
		// device is still in exclusive mode
		return;
	}

	if (res != S_OK) {
		return;
	}

	const string prevStreamId = currentExclusiveStreamId;
	createExclusiveStreamListener(false);
	if (!currentExclusiveStreamId.empty() && prevStreamId != currentExclusiveStreamId) {
		logger.debug(L"stream id changed, reconnect");
		state = State::eRECONNECT_NEEDED;
		return;
	}

	const auto changes = sessionEventsWrapper.grabChanges();
	switch (changes.disconnectionReason) {
		using DR = utils::AudioSessionEventsImpl::DisconnectionReason;
	case DR::eNONE:
		break;
	case DR::eUNAVAILABLE:
		state = State::eDEVICE_CONNECTION_ERROR;
		break;
	case DR::eRECONNECT:
		state = State::eRECONNECT_NEEDED;
		logger.debug(L"exclusive format changed");
		break;
	case DR::eEXCLUSIVE:
		// if eEXCLUSIVE happened, then the stream we were listening to was in shared-mode,
		// but now we probably have exclusive stream
		state = State::eDEVICE_IS_EXCLUSIVE;
		createExclusiveStreamListener(true);
		break;
	}
}

std::optional<utils::MediaDeviceWrapper> CaptureManager::getDevice(DataSource type, const string& id) {
	switch (type) {
	case DataSource::eDEFAULT_INPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
	case DataSource::eDEFAULT_OUTPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
	case DataSource::eID:
		return enumerator.getDevice(id);
	}

	return { };
}

string CaptureManager::makeFormatString(MyWaveFormat waveFormat) {
	using Format = utils::WaveDataFormat;

	if (waveFormat.format == Format::eINVALID) {
		return L"<invalid>";
	}

	utils::BufferPrinter bp;

	switch (waveFormat.format) {
	case Format::ePCM_S16:
		bp.append(L"PCM 16b");
		break;
	case Format::ePCM_F32:
		bp.append(L"PCM 32b");
		break;
	case Format::eINVALID: ;
	}

	bp.append(L", {} Hz, ", waveFormat.samplesPerSec);

	if (waveFormat.channelLayout.getName().empty()) {
		bp.append(L"unknown layout: {} recognized channels", waveFormat.channelLayout.getChannelsOrderView().size());
	} else {
		bp.append(L"{}", waveFormat.channelLayout.getName());
	}

	return string{ bp.getBufferView() };
}

void CaptureManager::createExclusiveStreamListener(bool makeLogMessage) {
	sessionEventsWrapper.destruct();

	auto sessionManager = audioDeviceHandle.activateFor<IAudioSessionManager2>();
	if (!sessionManager.isValid()) {
		return;
	}

	auto sessionEnumerator = utils::GenericComWrapper<IAudioSessionEnumerator>{
		[&](auto ptr) {
			auto result = sessionManager.getPointer()->GetSessionEnumerator(ptr);
			return result == S_OK;
		}
	};
	if (!sessionEnumerator.isValid()) {
		return;
	}

	int sessionCount;
	auto result = sessionEnumerator.getPointer()->GetCount(&sessionCount);
	if (result != S_OK) {
		return;
	}

	utils::GenericComWrapper<IAudioSessionControl> foundControl;
	index sessionsFound = 0;
	for (int i = 0; i < sessionCount; ++i) {
		auto sessionControl = utils::GenericComWrapper<IAudioSessionControl>{
			[&](auto ptr) {
				auto res = sessionEnumerator.getPointer()->GetSession(i, ptr);
				return res == S_OK;
			}
		};

		if (!sessionControl.isValid()) {
			continue;
		}

		AudioSessionState sessionState = { };
		auto res = sessionControl.getPointer()->GetState(&sessionState);
		if (res != S_OK) {
			continue;
		}

		if (sessionState != AudioSessionStateActive) {
			continue;
		}

		if (sessionsFound > 0) {
			// then there are at least two active sessions
			// exclusive mode can't have more than one active session
			state = State::eRECONNECT_NEEDED;
			return;
		}

		sessionsFound++;
		foundControl = std::move(sessionControl);
	}

	constexpr sview unknownDeviceStreamId = L"unknown";

	if (sessionsFound == 0) {
		// this can happen when device is in exclusive mode,
		// but the owner process doesn't produce any sound
		currentExclusiveStreamId = { };
		sessionEventsWrapper.destruct();
		return;
	}

	// let's print name of the application to inform user

	utils::GenericComWrapper<IAudioSessionControl2> c2{
		[&](auto ptr) {
			auto res = foundControl.getPointer()
			                       ->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(ptr));
			return res == S_OK;
		}
	};

	sessionEventsWrapper.init(std::move(foundControl));

	if (!c2.isValid()) {
		logger.notice(
			L"Device '{} ({})' is in exclusive mode, owner process is unknown, device ID: {}",
			snapshot.description,
			snapshot.nameOnly,
			snapshot.id
		);

		return;
	}

	wchar_t* sessionInstanceId = nullptr;
	auto res = c2.getPointer()->GetSessionIdentifier(&sessionInstanceId);
	if (res == S_OK) {
		currentExclusiveStreamId = sessionInstanceId;
		CoTaskMemFree(sessionInstanceId);
	} else {
		currentExclusiveStreamId = unknownDeviceStreamId;
	}

	if (!makeLogMessage) {
		return;
	}

	DWORD processId;
	res = c2.getPointer()->GetProcessId(&processId);

	if (res != S_OK) {
		logger.notice(
			L"Device '{} ({})' is in exclusive mode, owner process is unknown, device ID: {}",
			snapshot.description,
			snapshot.nameOnly,
			snapshot.id
		);
		return;
	}

	const auto processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processId);
	if (processHandle == nullptr) {
		logger.notice(
			L"Device '{} ({})' is in exclusive mode, owner process exe is unknown (process id is {}), device ID: {}",
			snapshot.description,
			snapshot.nameOnly,
			processId,
			snapshot.id
		);
		return;
	}

	constexpr index buffSize = 1024;
	wchar_t processNameBuffer[buffSize];
	DWORD processNameBufferLength = buffSize;
	const auto success = QueryFullProcessImageNameW(
		processHandle,
		0,
		processNameBuffer,
		&processNameBufferLength
	);
	CloseHandle(processHandle);

	if (!success) {
		logger.notice(
			L"Device '{} ({})' is in exclusive mode, owner process exe is unknown (process id is {}), device ID: {}",
			snapshot.description,
			snapshot.nameOnly,
			processId,
			snapshot.id
		);
		return;
	}

	logger.notice(
		L"Device '{} ({})' is in exclusive mode, owner is '{}' (process id is {}), device ID: {}",
		snapshot.description,
		snapshot.nameOnly,
		processNameBuffer,
		processId,
		snapshot.id
	);
}
