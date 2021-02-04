/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CaptureManager.h"

using namespace audio_analyzer;

using ComException = ::rxtd::common::winapi_wrappers::ComException;

void CaptureManager::setBufferSizeInSec(double value) {
	bufferSizeSec = std::clamp(value, 0.0, 1.0);
}

void CaptureManager::setSource(const SourceDesc& desc) {
	snapshot.state = setSourceAndGetState(desc);
}

void CaptureManager::disconnect() {
	if (getState() != State::eOK) {
		return;
	}

	snapshot.state = State::eMANUALLY_DISCONNECTED;
	audioCaptureClient = {};
	sessionEventsWrapper = {};
	sessionEventsWrapper = {};
	renderClient = {};
}

CaptureManager::State CaptureManager::setSourceAndGetState(const SourceDesc& desc) {
	auto deviceOpt = getDevice(desc);

	if (!deviceOpt.has_value()) {
		switch (desc.type) {
		case SourceDesc::Type::eDEFAULT_INPUT:
			logger.error(L"Default input audio device is not found");
			break;
		case SourceDesc::Type::eDEFAULT_OUTPUT:
			logger.error(L"Default output audio device is not found");
			break;
		case SourceDesc::Type::eID:
			logger.error(L"Audio device with id '{}' is not found", desc.id);
			break;
		}

		return State::eDEVICE_CONNECTION_ERROR;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	snapshot.id = audioDeviceHandle.getId();
	snapshot.description = deviceInfo.desc;
	snapshot.name = version < Version::eVERSION2 ? deviceInfo.fullFriendlyName : deviceInfo.name;
	snapshot.nameOnly = deviceInfo.name;

	try {
		auto testAudioClient = audioDeviceHandle.openAudioClient();
		testAudioClient.testExclusive();

		// reset lastExclusiveProcessId to invalid value
		lastExclusiveProcessId = -1;

		auto audioClient = audioDeviceHandle.openAudioClient();
		snapshot.type = audioClient.getType();
		snapshot.format = audioClient.getFormat();

		if (snapshot.format.channelLayout.ordered().empty()) {
			logger.error(L"zero known channels are found in the current channel layout");
			return State::eDEVICE_CONNECTION_ERROR;
		}

		snapshot.formatString = makeFormatString(snapshot.format);
		snapshot.channelsString.clear();
		auto channels = snapshot.format.channelLayout.ordered();
		for (int i = 0; i < channels.size() - 1; ++i) {
			snapshot.channelsString += ChannelUtils::getTechnicalName(channels[i]);
			snapshot.channelsString += L',';
		}
		snapshot.channelsString += ChannelUtils::getTechnicalName(channels.back());

		channelMixer.setLayout(snapshot.format.channelLayout);

		audioCaptureClient = audioClient.openCapture(bufferSizeSec);

		try {
			sessionEventsWrapper.listenTo(audioClient, true);
		} catch (ComException& e) {
			logger.error(L"Can't create session listener: error {}, caused by {}", e.getCode(), e.getSource());
		}

		audioClient.throwOnError(audioClient.ref().Start(), L"IAudioClient.Start()");

	} catch (wasapi_wrappers::FormatException&) {
		logger.error(L"Can't read device format");
		return State::eDEVICE_CONNECTION_ERROR;
	} catch (ComException& e) {
		if (e.getCode().code == AUDCLNT_E_DEVICE_IN_USE) {
			// #createExclusiveStreamListener can change state,
			// so I set state beforehand
			// and return whatever #createExclusiveStreamListener has set the state to
			snapshot.state = State::eDEVICE_IS_EXCLUSIVE;
			createExclusiveStreamListener();
			return snapshot.state;
		}

		logger.error(L"Can't connect to device: error {}, caused by {}", e.getCode(), e.getSource());
		return State::eDEVICE_CONNECTION_ERROR;
	}

	try {
		// This fixes the issue with values not updating in periods of silence
		// In some windows versions when no one is rendering sound IAudioCaptureClient just doesn't receive new audio frames
		// Since this plugin doesn't implement independent clock and relies solely on audio frames to measure time
		// this behaviour causes all values to stop updating.
		// Open IAudioRenderClient ensures that windows always feeds everyone with empty audio frames.
		//

		auto audioClient = audioDeviceHandle.openAudioClient();
		renderClient = audioClient.openRender();
		audioClient.throwOnError(audioClient.ref().Start(), L"IAudioClient.Start()");

	} catch (wasapi_wrappers::FormatException&) {
		logger.error(L"Can't create silent renderer: format error");
		return State::eDEVICE_CONNECTION_ERROR;
	} catch (ComException& e) {
		if (e.getCode().code == AUDCLNT_E_DEVICE_IN_USE) {
			// #createExclusiveStreamListener can change state,
			// so I set state beforehand
			// and return whatever #createExclusiveStreamListener has set the state to
			snapshot.state = State::eDEVICE_IS_EXCLUSIVE;
			createExclusiveStreamListener();
			return snapshot.state;
		}

		logger.error(L"Can't create silent renderer: error {}, caused by {}", e.getCode(), e.getSource());
	}

	return State::eOK;
}

bool CaptureManager::capture() {
	if (snapshot.state != State::eOK) {
		return false;
	}

	bool anyCaptured = false;
	channelMixer.reset();

	while (true) {
		const auto queryResult = audioCaptureClient.readBuffer();

		switch (queryResult) {
		case S_OK:
			anyCaptured = true;
			channelMixer.saveChannelsData(audioCaptureClient.getBuffer());
			continue;

		case AUDCLNT_S_BUFFER_EMPTY:
		case AUDCLNT_E_BUFFER_ERROR:
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
			break;
		default:
			logger.warning(L"Unexpected buffer query error code {}", queryResult);
		}

		const auto changes = sessionEventsWrapper.grabChanges();
		switch (changes.disconnectionReason) {
			using DR = wasapi_wrappers::AudioSessionEventsImpl::DisconnectionReason;
		case DR::eNONE:
			break;
		case DR::eUNAVAILABLE:
			snapshot.state = State::eDEVICE_CONNECTION_ERROR;
			break;
		case DR::eRECONNECT:
			snapshot.state = State::eRECONNECT_NEEDED;
			break;
		case DR::eEXCLUSIVE:
			snapshot.state = State::eDEVICE_IS_EXCLUSIVE;
			createExclusiveStreamListener();
			break;
		}

		break;
	}

	channelMixer.createAuto();

	return anyCaptured;
}

void CaptureManager::tryToRecoverFromExclusive() {
	const auto changes = sessionEventsWrapper.grabChanges();
	switch (changes.disconnectionReason) {
		using DR = wasapi_wrappers::AudioSessionEventsImpl::DisconnectionReason;
	case DR::eNONE:
		break;
	case DR::eUNAVAILABLE:
		snapshot.state = State::eDEVICE_CONNECTION_ERROR;
		return;
	case DR::eRECONNECT:
		snapshot.state = State::eRECONNECT_NEEDED;
		return;
	case DR::eEXCLUSIVE:
		// if eEXCLUSIVE happened, then the stream we were listening to was shared,
		// but now we probably have exclusive stream
		snapshot.state = State::eDEVICE_IS_EXCLUSIVE;
		createExclusiveStreamListener();
		return;
	}

	try {
		auto activeSessions = getActiveSessions();
		if (activeSessions.empty()) {
			return;
		}
		if (activeSessions.size() > 1) {
			snapshot.state = State::eRECONNECT_NEEDED;
			return;
		}

		GenericComWrapper<IAudioSessionControl2> c2{
			[&](auto ptr) {
				auto res = activeSessions[0].ref().QueryInterface<IAudioSessionControl2>(ptr);
				return res == S_OK;
			}
		};

		if (!c2.isValid()) {
			// can't get process id, so just skip
			return;
		}

		DWORD processId;
		const auto res = c2.ref().GetProcessId(&processId);

		if (res != S_OK) {
			// can't get process id, so just skip
			return;
		}

		if (processId == lastExclusiveProcessId) {
			return;
		}

		// process id changed, so previous exclusive stream must have been closed by now
		snapshot.state = State::eRECONNECT_NEEDED;
	} catch (ComException&) { }
}

std::optional<wasapi_wrappers::MediaDeviceWrapper> CaptureManager::getDevice(const SourceDesc& desc) {
	switch (desc.type) {
	case SourceDesc::Type::eDEFAULT_INPUT:
		return enumeratorWrapper.getDefaultDevice(wasapi_wrappers::MediaDeviceType::eINPUT);
	case SourceDesc::Type::eDEFAULT_OUTPUT:
		return enumeratorWrapper.getDefaultDevice(wasapi_wrappers::MediaDeviceType::eOUTPUT);
	case SourceDesc::Type::eID:
		return enumeratorWrapper.getDeviceByID(desc.id);
	}

	return {};
}

string CaptureManager::makeFormatString(wasapi_wrappers::WaveFormat waveFormat) {
	common::buffer_printer::BufferPrinter bp;

	if (waveFormat.channelLayout.getName().empty()) {
		bp.append(L"{} channels", waveFormat.channelLayout.ordered().size());
	} else {
		bp.append(L"{}", waveFormat.channelLayout.getName());
	}

	bp.append(L", {} Hz", waveFormat.samplesPerSec);

	return string{ bp.getBufferView() };
}

void CaptureManager::createExclusiveStreamListener() {
	sessionEventsWrapper.destruct();

	GenericComWrapper<IAudioSessionControl> session;
	try {
		auto activeSessions = getActiveSessions();

		if (activeSessions.empty()) {
			// this can happen when device is in exclusive mode,
			// but the owner process doesn't produce any sound
			lastExclusiveProcessId = -1;
			return;
		}

		if (activeSessions.size() > 1) {
			// exclusive mode only allows one active session
			lastExclusiveProcessId = -1;
			snapshot.state = State::eRECONNECT_NEEDED;
			return;
		}

		session = std::move(activeSessions.front());
	} catch (ComException& e) {
		logger.error(L"Can't get session list: error {}, caused by {}", e.getCode(), e.getSource());
		return;
	}

	// let's print name of the application to inform user

	std::optional<index> processId;
	string processExe;

	try {
		auto throwOnError = [](HRESULT code) {
			if (code != S_OK) {
				throw ComException{ code, {} };
			}
		};

		GenericComWrapper<IAudioSessionControl2> c2{
			[&](auto ptr) {
				throwOnError(session.ref().QueryInterface(ptr));
				return true;
			}
		};


		try {
			sessionEventsWrapper.listenTo(std::move(session), false);
		} catch (ComException& e) {
			logger.error(L"Can't create session listener for exclusive state monitoring: error {}, caused by {}", e.getCode(), e.getSource());
		}


		DWORD dwProcessId;
		throwOnError(c2.ref().GetProcessId(&dwProcessId));

		processId = dwProcessId;
		lastExclusiveProcessId = dwProcessId;

		const auto processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, dwProcessId);
		if (processHandle != nullptr) {
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

			if (success) {
				processExe = processNameBuffer;
			}
		}
	} catch (ComException&) { }

	common::buffer_printer::BufferPrinter bp;

	bp.append(
		L"Device '{} ({})' is in exclusive mode, owner exe is ",
		snapshot.description,
		snapshot.nameOnly
	);

	if (processExe.empty()) {
		bp.append(L"unknown");
	} else {
		bp.append(L"'{}'", processExe);
	}

	bp.append(L", process id is ");

	if (processId.has_value()) {
		bp.append(L"{}", processId.value());
	} else {
		bp.append(L"unknown");
	}

	bp.append(L", device ID is '{}'", snapshot.id);
	logger.notice(L"{}", bp.getBufferView());
}

std::vector<common::winapi_wrappers::GenericComWrapper<IAudioSessionControl>> CaptureManager::getActiveSessions() noexcept(false) {
	std::vector<GenericComWrapper<IAudioSessionControl>> result;

	auto sessionManager = audioDeviceHandle.activateFor<IAudioSessionManager2>();

	auto sessionEnumerator = GenericComWrapper<IAudioSessionEnumerator>{
		[&](auto ptr) {
			auto res = sessionManager.ref().GetSessionEnumerator(ptr);
			return res == S_OK;
		}
	};
	if (!sessionEnumerator.isValid()) {
		return {};
	}

	int sessionCount;
	auto res = sessionEnumerator.ref().GetCount(&sessionCount);
	if (res != S_OK) {
		return {};
	}

	for (int i = 0; i < sessionCount; ++i) {
		auto sessionControl = GenericComWrapper<IAudioSessionControl>{
			[&](auto ptr) {
				res = sessionEnumerator.ref().GetSession(i, ptr);
				return res == S_OK;
			}
		};

		if (!sessionControl.isValid()) {
			continue;
		}

		AudioSessionState sessionState = {};
		res = sessionControl.ref().GetState(&sessionState);
		if (res != S_OK) {
			continue;
		}

		if (sessionState != AudioSessionStateActive) {
			continue;
		}

		result.push_back(std::move(sessionControl));
	}

	return result;
}
