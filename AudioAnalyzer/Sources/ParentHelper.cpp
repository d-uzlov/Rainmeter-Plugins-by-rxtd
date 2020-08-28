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
	utils::Rainmeter::Logger _logger,
	const utils::OptionMap& threadingMap,
	index _legacyNumber
) {
	mainFields.logger = std::move(_logger);

	constFields.legacyNumber = _legacyNumber;

	if (!enumerator.isValid()) {
		mainFields.logger.error(L"Fatal error: can't create IMMDeviceEnumerator");
		throw std::exception{ };
	}

	updateDeviceListStrings();

	if (const auto threadingPolicy = threadingMap.get(L"policy").asIString(L"none");
		threadingPolicy == L"none") {
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
		constFields.updateTime = threadingMap.get(L"updateTime").asFloat(1.0 / 120.0);
		constFields.updateTime = std::clamp(constFields.updateTime, 1.0 / 200.0, 1.0);

		const double defaultBufferSize = std::max(constFields.updateTime * 2.0, 0.5);
		bufferSize = threadingMap.get(L"bufferSize").asFloat(defaultBufferSize);
		bufferSize = std::clamp(bufferSize, 1.0 / 30.0, 4.0);
	}

	mainFields.captureManager.setLogger(mainFields.logger);
	mainFields.captureManager.setLegacyNumber(constFields.legacyNumber);
	mainFields.captureManager.setBufferSizeInSec(bufferSize);
}

void ParentHelper::setParams(
	std::optional<RequestedDeviceDescription> device,
	std::optional<ParamParser::ProcessingsInfoMap> patches
) {
	if (!device.has_value()) {
		stopThread();
		mainFields.captureManager.disconnect();
		mainFields.settings.device = { };
		return;
	}

	auto requestLock = getRequestLock();

	requestFields.settings.device = std::move(device).value();
	requestFields.settings.patches = std::move(patches);

	if (constFields.useThreading && !requestFields.thread.joinable()) {
		threadSafeFields.stopRequest.exchange(false);
		requestFields.thread = std::thread{ [this]() { threadFunction(); } };
	}
}

void ParentHelper::update(Snapshot& snap) {
	if (!constFields.useThreading) {
		pUpdate();
	}

	auto requestLock = getRequestLock();

	if (requestFields.snapshotIsUpdated) {
		std::swap(requestFields.snapshot.dataSnapshot, snap.dataSnapshot);
	} else {
		snap = requestFields.snapshot;
		requestFields.snapshotIsUpdated = true;
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
	const auto sleepTime = 1.0s * constFields.updateTime;

	const auto res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	auto mainLock = getMainLock();

	if (res != S_OK) {
		mainFields.logger.error(L"separate thread: CoInitializeEx failed");
		return;
	}

	mainFields.logger.debug(L"thread started");

	while (true) {
		if (threadSafeFields.stopRequest.load()) {
			break;
		}

		pUpdate();

		if (threadSafeFields.stopRequest.load()) {
			break;
		}

		mainFields.sleepVariable.wait_for(mainLock, sleepTime);
	}

	mainFields.logger.debug(L"threadEnded");

	CoUninitialize();
}

void ParentHelper::pUpdate() {
	bool needToUpdateDevice = false;
	bool needToUpdateHandlers = false;

	{
		auto requestLock = getRequestLock();
		if (requestFields.settings.device != mainFields.settings.device) {
			mainFields.settings.device = requestFields.settings.device;
			needToUpdateDevice = true;
		}
		if (requestFields.settings.patches.has_value()) {
			mainFields.settings.patches = std::exchange(requestFields.settings.patches, { }).value();
			needToUpdateHandlers = true;
		}
	}

	const auto changes = threadSafeFields.notificationClient.takeChanges();

	using DDC = utils::CMMNotificationClient::DefaultDeviceChange;
	using DS = CaptureManager::DataSource;
	DDC defaultDeviceChange{ };
	switch (mainFields.settings.device.type) {
	case DS::eDEFAULT_INPUT:
		defaultDeviceChange = changes.defaultCapture;
		break;
	case DS::eDEFAULT_OUTPUT:
		defaultDeviceChange = changes.defaultRender;
		break;
	case DS::eID:
		defaultDeviceChange = DDC::eNONE;
		break;
	}

	switch (defaultDeviceChange) {
	case DDC::eNONE: break;
	case DDC::eCHANGED:
		needToUpdateDevice = true;
		break;
	case DDC::eNO_DEVICE: {
		const sview io = mainFields.settings.device.type == DS::eDEFAULT_INPUT ? L"input" : L"output";
		mainFields.logger.error(
			L"Requested default {} audio device, but all {} devices has been disconnected",
			io, io
		);

		// todo
		// requestFields.snapshot.deviceIsAvailable = false;
		// requestFields.snapshotIsUpdated = false;
		break;
	}
	}

	if (!changes.devices.empty()) {
		{
			auto requestLock = getRequestLock();
			updateDeviceListStrings();
		}

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

	const bool anyCaptured = mainFields.captureManager.capture();

	bool formatChanged = false;
	if (needToUpdateDevice) {
		formatChanged = updateDevice();
		needToUpdateHandlers = formatChanged;

	}
	if (needToUpdateHandlers) {
		updateProcessings();
	}

	if (needToUpdateDevice || needToUpdateHandlers) {
		auto requestLock = getRequestLock();
		requestFields.snapshotIsUpdated = false;
		requestFields.snapshot.deviceIsAvailable = mainFields.captureManager.getState() == CaptureManager::State::eOK;

		if (formatChanged) {
			// todo check this
			// it's important that if device is not available
			// then #snapshot is not updated
			requestFields.snapshot.deviceInfoSnapshot = mainFields.captureManager.getSnapshot();
		}

		mainFields.orchestrator.configureSnapshot(requestFields.snapshot.dataSnapshot);
	}

	if (anyCaptured) {
		mainFields.orchestrator.process(mainFields.captureManager.getChannelMixer());

		{
			auto requestLock = getRequestLock();
			mainFields.orchestrator.exchangeData(requestFields.snapshot.dataSnapshot);
		}
	}
}

bool ParentHelper::updateDevice() {
	const auto oldFormat = mainFields.captureManager.getSnapshot().format;
	mainFields.captureManager.setSource(
		mainFields.settings.device.type,
		mainFields.settings.device.id
	);

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
	if (constFields.legacyNumber < 104) {
		requestFields.snapshot.deviceListInput = enumerator.legacy_makeDeviceString(utils::MediaDeviceType::eINPUT);
		requestFields.snapshot.deviceListOutput = enumerator.legacy_makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	} else {
		requestFields.snapshot.deviceListInput = enumerator.makeDeviceString(utils::MediaDeviceType::eINPUT);
		requestFields.snapshot.deviceListOutput = enumerator.makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	}
}

std::unique_lock<std::mutex> ParentHelper::getMainLock() {
	auto lock = std::unique_lock<std::mutex>{ mainFields.mutex, std::defer_lock };
	if (constFields.useThreading) {
		lock.lock();
	}
	return lock;
}

std::unique_lock<std::mutex> ParentHelper::getRequestLock() {
	auto lock = std::unique_lock<std::mutex>{ requestFields.mutex, std::defer_lock };
	if (constFields.useThreading) {
		lock.lock();
	}
	return lock;
}
