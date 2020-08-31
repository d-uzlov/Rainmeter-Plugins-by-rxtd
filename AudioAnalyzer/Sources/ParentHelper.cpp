/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParentHelper.h"

using namespace audio_analyzer;

ParentHelper::~ParentHelper() {
	try {
		stopThread();
	} catch (...) {
	}
}

void ParentHelper::init(
	utils::Rainmeter _rain,
	utils::Rainmeter::Logger _logger,
	const utils::OptionMap& threadingMap,
	index _legacyNumber
) {
	mainFields.rain = std::move(_rain);
	mainFields.logger = std::move(_logger);

	constFields.legacyNumber = _legacyNumber;

	if (!enumeratorWrapper.isValid()) {
		mainFields.logger.error(L"Fatal error: can't create IMMDeviceEnumerator");
		throw std::exception{ };
	}

	updateDeviceListStrings();

	if (const auto threadingPolicy = threadingMap.get(L"policy").asIString(L"separateThread");
		threadingPolicy == L"uiThread") {
		constFields.useThreading = false;
	} else if (threadingPolicy == L"separateThread") {
		constFields.useThreading = true;
		threadSafeFields.notificationClient.setCallback([this]() { wakeThreadUp(); });
	} else {
		mainFields.logger.error(L"Fatal error: Threading: unknown policy '{}'");
		throw std::exception{ };
	}

	const double warnTime = threadingMap.get(L"warnTime").asFloat(-1.0);
	const double killTimeout = std::clamp(threadingMap.get(L"killTimeout").asFloat(33.0), 0.01, 33.0);

	mainFields.orchestrator.setLogger(mainFields.logger);
	mainFields.orchestrator.setWarnTime(warnTime);
	mainFields.orchestrator.setKillTimeout(killTimeout);

	double bufferSize = 1.0;
	if (constFields.useThreading) {
		constFields.updateTime = threadingMap.get(L"updateTime").asFloat(1.0 / 60.0);
		constFields.updateTime = std::clamp(constFields.updateTime, 1.0 / 200.0, 1.0);

		const double defaultBufferSize = std::max(constFields.updateTime * 4.0, 0.5);
		bufferSize = threadingMap.get(L"bufferSize").asFloat(defaultBufferSize);
		bufferSize = std::clamp(bufferSize, 1.0 / 30.0, 4.0);
	}

	mainFields.captureManager.setLogger(mainFields.logger);
	mainFields.captureManager.setLegacyNumber(constFields.legacyNumber);
	mainFields.captureManager.setBufferSizeInSec(bufferSize);

	mainFields.useLocking = constFields.useThreading;
	requestFields.useLocking = constFields.useThreading;
	snapshot.setThreading(constFields.useThreading);
}

void ParentHelper::setInvalid() {
	stopThread();

	mainFields.captureManager.disconnect();
	mainFields.orchestrator.reset();
	mainFields.settings.device = { };

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
			threadSafeFields.stopRequest.exchange(false);
			requestFields.thread = std::thread{ [this]() { threadFunction(); } };
		}
	}
}

void ParentHelper::update() {
	if (!constFields.useThreading) {
		pUpdate();
	}
}

void ParentHelper::wakeThreadUp() {
	if (!constFields.useThreading) {
		return;
	}

	try {
		auto lock = std::unique_lock<std::mutex>{ mainFields.mutex, std::defer_lock };
		const bool locked = lock.try_lock();
		if (locked) {
			mainFields.sleepVariable.notify_one();
		}
	} catch (...) {
	}
}

void ParentHelper::stopThread() {
	if (!constFields.useThreading) {
		return;
	}

	threadSafeFields.stopRequest.exchange(true);
	wakeThreadUp();

	if (requestFields.thread.joinable()) {
		requestFields.thread.join();
	}
}

void ParentHelper::threadFunction() {
	using namespace std::chrono_literals;
	using clock = std::chrono::high_resolution_clock;
	static_assert(clock::is_steady);

	const auto res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	auto mainLock = mainFields.getLock();

	if (res != S_OK) {
		mainFields.logger.error(L"separate thread: CoInitializeEx failed");
		return;
	}

	const auto sleepTime = std::chrono::duration_cast<clock::duration>(1.0s * constFields.updateTime);
	// mainFields.sleepVariable.wait_for(mainLock, sleepTime);

	while (true) {
		if (threadSafeFields.stopRequest.load()) {
			break;
		}

		auto nextWakeTime = clock::now() + sleepTime;

		pUpdate();

		if (threadSafeFields.stopRequest.load()) {
			break;
		}

		mainFields.sleepVariable.wait_until(mainLock, nextWakeTime);
	}

	CoUninitialize();
}

void ParentHelper::pUpdate() {
	bool needToUpdateDevice = false;
	bool needToUpdateHandlers = !mainFields.orchestrator.isValid();

	{
		auto requestLock = requestFields.getLock();
		if (requestFields.settings.device.has_value()) {
			mainFields.settings.device = std::exchange(requestFields.settings.device, { }).value();
			needToUpdateDevice = true;
		}
		if (requestFields.settings.patches.has_value()) {
			mainFields.settings.patches = std::exchange(requestFields.settings.patches, { }).value();
			needToUpdateHandlers = true;
		}
		if (requestFields.settings.callbacks.has_value()) {
			mainFields.callbacks = std::exchange(requestFields.settings.callbacks, { }).value();
		}
	}

	const auto changes = threadSafeFields.notificationClient.takeChanges();

	using DDC = utils::CMMNotificationClient::DefaultDeviceChange;
	using ST = CaptureManager::SourceDesc::Type;
	DDC defaultDeviceChange{ };
	switch (mainFields.settings.device.type) {
	case ST::eDEFAULT_INPUT:
		defaultDeviceChange = changes.defaultCapture;
		break;
	case ST::eDEFAULT_OUTPUT:
		defaultDeviceChange = changes.defaultRender;
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
		{
			auto lock = snapshot.data.getLock();
			snapshot.data._ = { };
			snapshot.deviceIsAvailable = false;
		}

		const sview io = mainFields.settings.device.type == ST::eDEFAULT_INPUT ? L"input" : L"output";
		mainFields.logger.error(
			L"Either all {} devices has been disconnected, or windows audio service was stopped",
			io
		);
		mainFields.captureManager.disconnect();
		mainFields.orchestrator.reset();

		break;
	}
	}

	if (!changes.devices.empty()) {
		{
			auto lock = snapshot.deviceLists.getLock();
			updateDeviceListStrings();
		}

		mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceListChange);

		if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_CONNECTION_ERROR
			&& changes.devices.count(mainFields.captureManager.getSnapshot().id) > 0) {
			needToUpdateDevice = true;
		}
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_IS_EXCLUSIVE) {
		mainFields.captureManager.tryToRecoverFromExclusive();
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		needToUpdateDevice = true;
	}

	if (!needToUpdateDevice && mainFields.captureManager.getState() != CaptureManager::State::eOK) {
		return;
	}

	// after we know if we need to reconnect to audio device
	// we grab data from current device buffer, so that wave flow don't hiccup
	// then instantly update device if need be
	//	so that on next update we have something in buffer
	// then process current captured data
	//	which may take some time, so we would miss some data if we didn't reconnect to device

	const bool anyCaptured = mainFields.captureManager.capture();

	if (needToUpdateDevice) {
		const bool formatChanged = reconnectToDevice();
		needToUpdateHandlers |= formatChanged;

		snapshot.deviceIsAvailable = mainFields.captureManager.getState() == CaptureManager::State::eOK;

		{
			auto diLock = snapshot.deviceInfo.getLock();
			snapshot.deviceInfo._ = mainFields.captureManager.getSnapshot();
		}

		mainFields.rain.executeCommandAsync(mainFields.callbacks.onDeviceChange);

	}
	if (needToUpdateHandlers) {
		updateProcessings();
		{
			auto dataLock = snapshot.data.getLock();
			mainFields.orchestrator.configureSnapshot(snapshot.data._);
		}
	}

	if (anyCaptured) {
		mainFields.orchestrator.process(mainFields.captureManager.getChannelMixer());

		{
			auto dataLock = snapshot.data.getLock();
			mainFields.orchestrator.exchangeData(snapshot.data._);
		}

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
		mainFields.settings.patches, constFields.legacyNumber,
		mainFields.captureManager.getSnapshot().format.samplesPerSec,
		mainFields.captureManager.getSnapshot().format.channelLayout
	);
}

void ParentHelper::updateDeviceListStrings() {
	auto& lists = snapshot.deviceLists;
	if (constFields.legacyNumber < 104) {
		lists.input = legacy_makeDeviceListString(utils::MediaDeviceType::eINPUT);
		lists.output = legacy_makeDeviceListString(utils::MediaDeviceType::eOUTPUT);
	} else {
		lists.input = makeDeviceListString(utils::MediaDeviceType::eINPUT);
		lists.output = makeDeviceListString(utils::MediaDeviceType::eOUTPUT);
	}
}

string ParentHelper::makeDeviceListString(utils::MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;
	}

	utils::BufferPrinter bp;
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
	auto appendFormat = [&](utils::MediaDeviceWrapper& device)-> bool {
		auto audioClient = device.openAudioClient();
		if (!audioClient.isValid()) {
			return false;
		}

		audioClient.readFormat();
		if (!audioClient.isFormatValid()) {
			return false;
		}

		auto format = audioClient.getFormat();
		bp.append(L"{};", format.samplesPerSec);

		auto layout = ChannelUtils::parseLayout(format.channelMask);
		if (layout.ordered().empty()) {
			bp.append(L"<unknown>;");
		} else {
			auto channels = layout.ordered();
			channels.remove_suffix(1);
			for (auto channel : channels) {
				bp.append(L"{},", ChannelUtils::getTechnicalName(channel));
			}
			bp.append(L"{};", ChannelUtils::getTechnicalName(layout.ordered().back()));
		}

		return true;
	};

	for (auto& device : collection) {
		auto id = device.readId();
		if (id.empty()) {
			continue;
		}

		bp.append(L"{}", id);

		const auto deviceInfo = device.readDeviceInfo();
		append(deviceInfo.name);
		append(deviceInfo.desc);
		append(deviceInfo.formFactor);

		bool formatAppendSuccess = appendFormat(device);
		if (!formatAppendSuccess) {
			append({ });
			append({ });
		}

		bp.append(L"\n");
	}

	result = bp.getBufferView();
	result.pop_back(); // removes \0
	result.pop_back(); // removes \n

	return result;
}

string ParentHelper::legacy_makeDeviceListString(utils::MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;;
	}

	utils::BufferPrinter bp;
	for (auto& device : collection) {
		auto id = device.readId();
		if (id.empty()) {
			continue;
		}

		const auto deviceInfo = device.readDeviceInfo();
		bp.append(L"{}", id);
		bp.append(L" ");
		bp.append(L"{}\n", deviceInfo.fullFriendlyName);
	}

	result = bp.getBufferView();
	result.pop_back();
	result.pop_back();

	return result;
}
