// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "ParentHelper.h"

using rxtd::audio_analyzer::ParentHelper;

ParentHelper::~ParentHelper() {
	threadSafeFields.notificationClient.ref().deinit(enumeratorWrapper);
	try {
		stopThread();
	} catch (...) { }
}

void ParentHelper::init(
	Rainmeter _rain,
	Logger _logger,
	const OptionMap& threadingMap,
	option_parsing::OptionParser& parser,
	Version version,
	bool suppressVolumeChange,
	sview devListChangeCallback
) {
	mainFields.rain = std::move(_rain);
	mainFields.logger = std::move(_logger);

	constFields.version = version;

	if (!enumeratorWrapper.isValid()) {
		mainFields.logger.error(L"Fatal error: can't create IMMDeviceEnumerator");
		throw std::runtime_error{ "" };
	}

	updateDeviceListStrings();
	mainFields.rain.executeCommandAsync(devListChangeCallback);

	threadSafeFields.notificationClient = {
		[](auto ptr) {
			*ptr = wasapi_wrappers::implementations::MediaDeviceListNotificationClient::create();
			return true;
		}
	};
	threadSafeFields.notificationClient.ref().init(enumeratorWrapper);

	if (const auto threadingPolicy = threadingMap.get(L"policy").asIString(L"separateThread");
		threadingPolicy == L"uiThread") {
		constFields.useThreading = false;
	} else if (threadingPolicy == L"separateThread") {
		constFields.useThreading = true;
		threadSafeFields.notificationClient.ref().setCallback(
			[this]() {
				wakeThreadUp();
			}
		);
	} else {
		mainFields.logger.error(L"Fatal error: Threading: unknown policy '{}'");
		throw std::runtime_error{ "" };
	}

	const double warnTime = parser.parse(threadingMap, L"warnTime").valueOr(-1.0);
	const double killTimeout = std::clamp(parser.parse(threadingMap, L"killTimeout").valueOr(33.0), 0.01, 33.0);

	mainFields.orchestrator.setLogger(mainFields.logger);
	mainFields.orchestrator.setWarnTime(warnTime);
	mainFields.orchestrator.setKillTimeout(killTimeout);

	double bufferSize = 1.0;
	if (constFields.useThreading) {
		double updateRate = parser.parse(threadingMap, L"updateRate").valueOr(60.0);
		updateRate = std::clamp(updateRate, 1.0, 200.0);
		constFields.updateTime = 1.0 / updateRate;

		const double defaultBufferSize = std::max(constFields.updateTime * 4.0, 0.5);
		bufferSize = parser.parse(threadingMap, L"bufferSize").valueOr(defaultBufferSize);
		bufferSize = std::clamp(bufferSize, 1.0 / 30.0, 4.0);
	}

	mainFields.captureManager.setSuppressVolumeChange(suppressVolumeChange);
	mainFields.captureManager.setLogger(mainFields.logger);
	mainFields.captureManager.setVersion(constFields.version);
	mainFields.captureManager.setBufferSizeInSec(bufferSize);

	requestFields.setUseLocking(constFields.useThreading);
	threadSleepFields.setUseLocking(constFields.useThreading);
	snapshot.setThreading(constFields.useThreading);
}

void ParentHelper::setInvalid() {
	requestFields.runGuarded(
		[&] {
			requestFields.disconnect = true;
		}
	);
	snapshot.deviceIsAvailable = false;
}

void ParentHelper::setParams(
	std::optional<Callbacks> callbacks,
	std::optional<CaptureManager::SourceDesc> device,
	std::optional<options::ParamHelper::ProcessingsInfoMap> patches
) {
	auto requestLock = requestFields.getLock();

	requestFields.settings.callbacks = std::move(callbacks);
	requestFields.settings.device = std::move(device);
	requestFields.settings.patches = std::move(patches);

	if (constFields.useThreading) {
		if (requestFields.thread.joinable()) {
			wakeThreadUp();
		} else {
			threadSleepFields.stopRequest = false;
			threadSleepFields.updateRequest = false;
			requestFields.thread = std::thread{
				[this]() {
					threadFunction();
				}
			};
		}
	}
}

void ParentHelper::update() {
	if (!constFields.useThreading) {
		pUpdate();
	}
}

void ParentHelper::assertNoExceptions() const {
	if (exceptionHappened) {
		throw std::runtime_error{ "" };
	}
}

void ParentHelper::wakeThreadUp() {
	try {
		threadSleepFields.runGuarded(
			[&] {
				threadSleepFields.updateRequest = true;
				threadSleepFields.sleepVariable.notify_one();
			}
		);
	} catch (...) { }
}

void ParentHelper::stopThread() {
	if (!requestFields.thread.joinable()) {
		return;
	}

	threadSleepFields.runGuarded(
		[&] {
			threadSleepFields.stopRequest = true;
			threadSleepFields.sleepVariable.notify_one();
		}
	);

	requestFields.thread.join();
}

void ParentHelper::threadFunction() {
	using namespace std::chrono_literals;
	using clock = std::chrono::high_resolution_clock;
	static_assert(clock::is_steady);

	const auto res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	if (res != S_OK) {
		mainFields.logger.error(L"separate thread: CoInitializeEx failed");
		return;
	}

	const auto sleepTime = std::chrono::duration_cast<clock::duration>(1.0s * constFields.updateTime);

	try {
		while (true) {
			auto nextWakeTime = clock::now() + sleepTime;
			pUpdate();

			{
				auto sleepLock = threadSleepFields.getLock();
				if (threadSleepFields.stopRequest) {
					break;
				}
				if (!threadSleepFields.updateRequest) {
					threadSleepFields.sleepVariable.wait_until(sleepLock, nextWakeTime);
					if (threadSleepFields.stopRequest) {
						break;
					}
				} else {
					threadSleepFields.updateRequest = false;
				}
			}
		}
	} catch (std::runtime_error&) {
		mainFields.logger.error(L"separate thread: update unexpectedly failed");
		exceptionHappened = true;
	}

	CoUninitialize();
}

void ParentHelper::pUpdate() {
	bool needToUpdateDevice = false;
	bool needToUpdateHandlers = !mainFields.orchestrator.isValid();

	{
		auto requestLock = requestFields.getLock();
		if (requestFields.disconnect) {
			doDisconnectRoutine();
			requestFields.disconnect = false;
			return;
		}

		if (requestFields.settings.device.has_value()) {
			mainFields.settings.device = std::exchange(requestFields.settings.device, {}).value();
			needToUpdateDevice = true;
		}
		if (requestFields.settings.patches.has_value()) {
			mainFields.settings.patches = std::exchange(requestFields.settings.patches, {}).value();
			needToUpdateHandlers = true;
		}
		if (requestFields.settings.callbacks.has_value()) {
			mainFields.callbacks = std::exchange(requestFields.settings.callbacks, {}).value();
		}
	}

	const auto changes = threadSafeFields.notificationClient.ref().takeChanges();

	using DDC = wasapi_wrappers::implementations::MediaDeviceListNotificationClient::DefaultDeviceChange;
	using ST = CaptureManager::SourceDesc::Type;
	DDC defaultDeviceChange{};
	switch (mainFields.settings.device.type) {
	case ST::eDEFAULT_INPUT:
		defaultDeviceChange = changes.defaultInputChange;
		break;
	case ST::eDEFAULT_OUTPUT:
		defaultDeviceChange = changes.defaultOutputChange;
		break;
	case ST::eID:
		defaultDeviceChange = DDC::eNONE;
		break;
	}

	switch (defaultDeviceChange) {
	case DDC::eNONE: break;
	case DDC::eCHANGED:
		needToUpdateDevice = true;
		break;
	case DDC::eNO_DEVICE: {
		snapshot.deviceIsAvailable = false;

		const sview io = mainFields.settings.device.type == ST::eDEFAULT_INPUT ? L"input" : L"output";
		mainFields.logger.error(
			L"Either all {} devices has been disconnected, or windows audio service was stopped",
			io
		);
		doDisconnectRoutine();
		return;
	}
	}

	if (mainFields.settings.device.type == CaptureManager::SourceDesc::Type::eID
		&& changes.removed.find(mainFields.captureManager.getSnapshot().id) != changes.removed.end()) {
		mainFields.logger.warning(L"Specified device has been disabled or disconnected");
		mainFields.captureManager.disconnect();
		needToUpdateDevice = false;
		snapshot.deviceInfo.runGuarded(
			[&] {
				snapshot.deviceInfo._.state = mainFields.captureManager.getState();
			}
		);
		mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceDisconnected);
	} else if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_CONNECTION_ERROR
		&& changes.stateChanged.count(mainFields.captureManager.getSnapshot().id) > 0) {
		needToUpdateDevice = true;
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_IS_EXCLUSIVE) {
		mainFields.captureManager.tryToRecoverFromExclusive();
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		needToUpdateDevice = true;
	}


	if (!changes.stateChanged.empty() || !changes.removed.empty()) {
		const bool deviceListChanged = updateDeviceListStrings();
		if (deviceListChanged) {
			mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceListChange);
		}
	}

	if (!needToUpdateDevice && mainFields.captureManager.getState() != CaptureManager::State::eOK) {
		return;
	}

	// after we know if we need to reconnect to audio device
	// we grab data from current device buffer, so that wave flow doesn't hiccup
	// then instantly update device if need be
	//	so that on next update we have something in buffer
	// then process current captured data
	//	which may take some time, so we would miss some data
	//	if we didn't reconnect to device before processing

	const bool anyCaptured = mainFields.captureManager.capture();

	if (mainFields.captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		needToUpdateDevice = true;
	}

	if (needToUpdateDevice) {
		const bool formatChanged = reconnectToDevice();
		needToUpdateHandlers |= formatChanged;

		snapshot.deviceIsAvailable = mainFields.captureManager.getState() == CaptureManager::State::eOK;
		if (!snapshot.deviceIsAvailable) {
			doDisconnectRoutine();
			return;
		}
		snapshot.deviceInfo.runGuarded(
			[&] {
				snapshot.deviceInfo._ = mainFields.captureManager.getSnapshot();
			}
		);
	}
	if (needToUpdateHandlers) {
		updateProcessings();
		snapshot.data.runGuarded(
			[&] {
				mainFields.orchestrator.configureSnapshot(snapshot.data._);
			}
		);
	}
	if (needToUpdateDevice) {
		// callback may want to use some data from snapshot.data,
		// that is updated in the "if (needToUpdateHandlers)" branch
		// so we call the callback in this separate if() branch
		switch (mainFields.captureManager.getState()) {
		case CaptureManager::State::eOK:
			mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceChange);
			break;
		case CaptureManager::State::eRECONNECT_NEEDED:
		case CaptureManager::State::eMANUALLY_DISCONNECTED: break;
		case CaptureManager::State::eDEVICE_CONNECTION_ERROR:
		case CaptureManager::State::eDEVICE_IS_EXCLUSIVE:
			mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceDisconnected);
			break;
		}
	}

	if (anyCaptured) {
		mainFields.orchestrator.process(mainFields.captureManager.getChannelMixer());
		snapshot.data.runGuarded(
			[&] {
				mainFields.orchestrator.exchangeData(snapshot.data._);
			}
		);
		mainFields.rain.executeCommandAsync(mainFields.callbacks.onUpdate);
	}
}

void ParentHelper::doDisconnectRoutine() {
	mainFields.captureManager.disconnect();
	mainFields.orchestrator.reset();
	snapshot.deviceInfo.runGuarded(
		[&] {
			snapshot.deviceInfo._.state = mainFields.captureManager.getState();
		}
	);
	mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceDisconnected);
}

bool ParentHelper::reconnectToDevice() {
	const auto oldFormat = mainFields.captureManager.getSnapshot().format;
	mainFields.captureManager.setSource(mainFields.settings.device);

	if (mainFields.captureManager.getState() != CaptureManager::State::eOK) {
		return false;
	}

	return oldFormat != mainFields.captureManager.getSnapshot().format;
}

void ParentHelper::updateProcessings() {
	mainFields.orchestrator.patch(
		mainFields.settings.patches, constFields.version,
		mainFields.captureManager.getSnapshot().format.samplesPerSec,
		mainFields.captureManager.getSnapshot().format.channelLayout.getOrdered()
	);
}

bool ParentHelper::updateDeviceListStrings() {
	string list = makeDeviceListString(MediaDeviceType::eINPUT);
	list += makeDeviceListString(MediaDeviceType::eOUTPUT);

	auto& wrapper = snapshot.deviceListWrapper;
	auto lock = wrapper.getLock();

	if (wrapper.list != list) {
		wrapper.list = list;
		return true;
	}

	return false;
}

rxtd::string ParentHelper::makeDeviceListString(MediaDeviceType type) {
	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return {};
	}

	buffer_printer::BufferPrinter bp;

	auto append = [&](sview str) {
		if (str.empty()) {
			bp.append(L"<unknown>");
		} else {
			for (auto c : str) {
				if (c == L'%') {
					bp.append(L"%percent");
				} else if (c == L';') {
					bp.append(L"%semicolon");
				} else if (c == L'/') {
					bp.append(L"%forwardslash");
				} else {
					bp.append(L"{}", c);
				}
			}
		}
		bp.append(L";");
	};

	// returns success
	auto appendFormat = [&](const wasapi_wrappers::WaveFormat& format) {
		bp.append(L"{};", format.samplesPerSec);

		if (format.channelLayout.ordered().empty()) {
			append({});
		} else {
			auto channels = format.channelLayout.ordered();
			for (auto channel : channels) {
				bp.append(L"{},", ChannelUtils::getTechnicalName(channel));
			}
			bp.append(L";");
		}
	};

	for (auto& device : collection) {
		using DeviceInfo = wasapi_wrappers::MediaDeviceHandle::DeviceInfo;

		DeviceInfo deviceInfo;
		wasapi_wrappers::AudioClientHandle audioClient;
		try {
			deviceInfo = device.readDeviceInfo();
			audioClient = device.openAudioClient();

			// if anything has gone wrong, then ignore this device
		} catch (winapi_wrappers::ComException&) {
			continue;
		} catch (wasapi_wrappers::FormatException&) {
			continue;
		}

		append(device.getId());

		append(deviceInfo.name);
		append(deviceInfo.desc);
		append(deviceInfo.formFactor);

		appendFormat(audioClient.getFormat());

		append(device.getType() == MediaDeviceType::eINPUT ? L"input" : L"output");

		bp.append(L"/");
	}

	return string{ bp.getBufferView() };
}
