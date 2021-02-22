/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParentHelper.h"

using namespace rxtd::audio_analyzer;

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
	Version version
) {
	mainFields.rain = std::move(_rain);
	mainFields.logger = std::move(_logger);

	constFields.version = version;

	if (!enumeratorWrapper.isValid()) {
		mainFields.logger.error(L"Fatal error: can't create IMMDeviceEnumerator");
		throw std::runtime_error{ "" };
	}

	updateDeviceListStrings();

	threadSafeFields.notificationClient = {
		[](auto ptr) {
			*ptr = wasapi_wrappers::MediaDeviceListNotificationClient::create();
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

	const double warnTime = threadingMap.get(L"warnTime").asFloat(-1.0);
	const double killTimeout = std::clamp(threadingMap.get(L"killTimeout").asFloat(33.0), 0.01, 33.0);

	mainFields.orchestrator.setLogger(mainFields.logger);
	mainFields.orchestrator.setWarnTime(warnTime);
	mainFields.orchestrator.setKillTimeout(killTimeout);

	double bufferSize = 1.0;
	if (constFields.useThreading) {
		double updateRate = threadingMap.get(L"updateRate").asFloat(60.0);;
		updateRate = std::clamp(updateRate, 1.0, 200.0);
		constFields.updateTime = 1.0 / updateRate;

		const double defaultBufferSize = std::max(constFields.updateTime * 4.0, 0.5);
		bufferSize = threadingMap.get(L"bufferSize").asFloat(defaultBufferSize);
		bufferSize = std::clamp(bufferSize, 1.0 / 30.0, 4.0);
	}

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
	std::optional<ParamParser::ProcessingsInfoMap> patches
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

	CoUninitialize();
}

void ParentHelper::pUpdate() {
	bool needToUpdateDevice = false;
	bool needToUpdateHandlers = !mainFields.orchestrator.isValid();

	{
		auto requestLock = requestFields.getLock();
		if (requestFields.disconnect) {
			mainFields.disconnected = true;

			mainFields.captureManager.disconnect();
			mainFields.orchestrator.reset();

			requestFields.settings.device = {};
			requestFields.settings.patches = {};
			requestFields.settings.callbacks = {};
			requestFields.disconnect = false;
		} else {
			if (requestFields.settings.device.has_value()) {
				mainFields.disconnected = false;
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
	}

	const auto changes = threadSafeFields.notificationClient.ref().takeChanges();

	if (!mainFields.disconnected) {
		using DDC = wasapi_wrappers::MediaDeviceListNotificationClient::DefaultDeviceChange;
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
			mainFields.captureManager.disconnect();
			mainFields.orchestrator.reset();
			snapshot.deviceInfo.runGuarded(
				[&] {
					snapshot.deviceInfo._.state = mainFields.captureManager.getState();
				}
			);
			mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceDisconnected);
			break;
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
	string input;
	string output;
	if (constFields.version < Version::eVERSION2) {
		input = legacy_makeDeviceListString(MediaDeviceType::eINPUT);
		output = legacy_makeDeviceListString(MediaDeviceType::eOUTPUT);
	} else {
		input = makeDeviceListString(MediaDeviceType::eINPUT);
		output = makeDeviceListString(MediaDeviceType::eOUTPUT);
	}

	auto lock = snapshot.deviceLists.getLock();

	auto& lists = snapshot.deviceLists;

	bool anyChanged = false;

	if (lists.input != input) {
		lists.input = input;
		anyChanged = true;
	}
	if (lists.output != output) {
		lists.output = output;
		anyChanged = true;
	}

	return anyChanged;
}

rxtd::string ParentHelper::makeDeviceListString(MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;
	}

	common::buffer_printer::BufferPrinter bp;
	auto append = [&](sview str, bool semicolon = true) {
		if (str.empty()) {
			bp.append(L"<unknown>");
		} else {
			bp.append(L"{}", str);
		}
		if (semicolon) {
			bp.append(L";");
		}
	};

	// returns success
	auto appendFormat = [&](wasapi_wrappers::MediaDeviceHandle& device) {
		try {
			auto audioClient = device.openAudioClient();

			auto& format = audioClient.getFormat();

			bp.append(L"{};", format.samplesPerSec);

			if (format.channelLayout.ordered().empty()) {
				append({});
			} else {
				auto channels = format.channelLayout.ordered();
				channels.remove_suffix(1);
				for (auto channel : channels) {
					bp.append(L"{},", ChannelUtils::getTechnicalName(channel));
				}
				bp.append(L"{};", ChannelUtils::getTechnicalName(format.channelLayout.ordered().back()));
			}

			return;
		} catch (wasapi_wrappers::FormatException&) { } catch (common::winapi_wrappers::ComException&) { }

		append({});
		append({});
	};

	for (auto& device : collection) {
		append(device.getId());

		try {
			const auto deviceInfo = device.readDeviceInfo();

			append(deviceInfo.name);
			append(deviceInfo.desc);
			append(deviceInfo.formFactor);
		} catch (common::winapi_wrappers::ComException&) {
			append({});
			append({});
			append({});
		}

		appendFormat(device);

		bp.append(L"\n");
	}

	result = bp.getBufferView();
	if (!result.empty()) {
		result.pop_back(); // removes \0
		result.pop_back(); // removes \n
	}

	return result;
}

rxtd::string ParentHelper::legacy_makeDeviceListString(MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;
	}

	common::buffer_printer::BufferPrinter bp;
	for (auto& device : collection) {
		try {
			const auto deviceInfo = device.readDeviceInfo();
			bp.append(L"{}", device.getId());
			bp.append(L" ");
			bp.append(L"{}\n", deviceInfo.fullFriendlyName);
		} catch (common::winapi_wrappers::ComException&) {}
	}

	result = bp.getBufferView();
	if (!result.empty()) {
		result.pop_back(); // removes \0
		result.pop_back(); // removes \n
	}

	return result;
}
