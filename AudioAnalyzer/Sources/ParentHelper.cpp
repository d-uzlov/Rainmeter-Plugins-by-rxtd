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
		requestFields.needToInitializeThread = true;
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
	std::optional<RequestedDeviceDescription> request,
	std::optional<ParamParser::ProcessingsInfoMap> patches
) {
	auto requestLock = getRequestLock();

	if (request.has_value()) {
		requestFields.requestedSource = std::move(request).value();
		requestFields.sourceWasUpdated = true;
	}

	if (patches.has_value()) {
		requestFields.patches = std::move(patches).value();
		requestFields.patchesWereUpdated = true;
	}

	if (requestFields.sourceWasUpdated || requestFields.patchesWereUpdated) {
		threadSafeFields.paramsWereUpdated.exchange(true);
	}

	if (requestFields.needToInitializeThread) {
		requestFields.needToInitializeThread = false;
		requestFields.thread = std::thread{ [this]() { threadFunction(); } };
	}
}

void ParentHelper::setInvalid() {
	stopThread();
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
	if (!constFields.useThreading || requestFields.needToInitializeThread) {
		return;
	}

	threadSafeFields.stopRequest.exchange(true);

	wakeThreadUp();

	requestFields.thread.join();

	requestFields.needToInitializeThread = true;
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


	CoUninitialize();
}

void ParentHelper::pUpdate() {
	if (threadSafeFields.paramsWereUpdated.load()) {
		auto requestLock = getRequestLock();
		updateDevice();
		threadSafeFields.paramsWereUpdated.exchange(false);
		mainFields.requestedSource = requestFields.requestedSource;

		requestFields.sourceWasUpdated = false;
		requestFields.patchesWereUpdated = false;

		// todo check why image in reset when I forgot to set optionsChanged = false
	}

	const auto changes = threadSafeFields.notificationClient.takeChanges();

	using DS = CaptureManager::DataSource;
	if (mainFields.requestedSource.type == DS::eDEFAULT_INPUT
		|| mainFields.requestedSource.type == DS::eDEFAULT_OUTPUT) {
		const auto change =
			mainFields.requestedSource.type == DS::eDEFAULT_INPUT
				? changes.defaultCapture
				: changes.defaultRender;
		switch (change) {
			using DDC = utils::CMMNotificationClient::DefaultDeviceChange;
		case DDC::eNONE: break;
		case DDC::eCHANGED: {
			auto requestLock = getRequestLock();

			updateDevice();
			break;
		}
		case DDC::eNO_DEVICE: {
			const sview io = mainFields.requestedSource.type == DS::eDEFAULT_INPUT ? L"input" : L"output";
			mainFields.logger.error(
				L"Requested default {} audio device, but all {} devices has been disconnected",
				io, io
			);
			auto requestLock = getRequestLock();
			requestFields.snapshot.deviceIsAvailable = false;
			requestFields.snapshotIsUpdated = false;
			break;
		}
		}
	}

	if (!changes.devices.empty()) {
		auto requestLock = getRequestLock();

		if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_CONNECTION_ERROR
			&& changes.devices.count(requestFields.snapshot.deviceInfoSnapshot.id) > 0) {
			updateDevice();
		}

		updateDeviceListStrings();
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eDEVICE_IS_EXCLUSIVE) {
		mainFields.captureManager.tryToRecoverFromExclusive();
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		auto requestLock = getRequestLock();

		updateDevice();
	}

	if (mainFields.captureManager.getState() != CaptureManager::State::eOK) {
		return;
	}

	const bool anyCaptured = mainFields.captureManager.capture();
	if (anyCaptured) {
		mainFields.orchestrator.process(mainFields.captureManager.getChannelMixer());

		{
			auto requestLock = getRequestLock();
			mainFields.orchestrator.exchangeData(requestFields.snapshot.dataSnapshot);
		}
	}

	if (mainFields.captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		updateDevice();
	}
}

void ParentHelper::updateDevice() {
	requestFields.snapshotIsUpdated = false;

	mainFields.captureManager.setSource(
		requestFields.requestedSource.type,
		requestFields.requestedSource.id
	);

	requestFields.snapshot.deviceIsAvailable = mainFields.captureManager.getState() == CaptureManager::State::eOK;
	if (!requestFields.snapshot.deviceIsAvailable) {
		return;
	}

	// todo if source format changed then update handlers

	// it's important that if device is not available
	// then #updateSnapshot is not called
	mainFields.captureManager.updateSnapshot(requestFields.snapshot.deviceInfoSnapshot);

	mainFields.orchestrator.patch(
		requestFields.patches, constFields.legacyNumber,
		requestFields.snapshot.deviceInfoSnapshot.format.samplesPerSec,
		requestFields.snapshot.deviceInfoSnapshot.format.channelLayout
	);

	mainFields.orchestrator.configureSnapshot(requestFields.snapshot.dataSnapshot);
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
