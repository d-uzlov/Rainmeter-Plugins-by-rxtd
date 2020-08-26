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
	if (useThreading && !needToInitializeThread) {
		stopRequest.exchange(true);
		try {
			{
				// todo tryLock
				auto fullStateLock = getFullStateLock();
				sleepVariable.notify_one();
			}
			thread.join();
		} catch (...) {
			thread.detach();
		}
	}
}

bool ParentHelper::init(
	utils::Rainmeter::Logger _logger,
	utils::OptionMap threadingMap,
	index _legacyNumber
) {
	logger = std::move(_logger);

	legacyNumber = _legacyNumber;

	if (!enumerator.isValid()) {
		logger.error(L"Fatal error: can't create IMMDeviceEnumerator");
		return false;
	}

	captureManager.setLogger(logger);
	captureManager.setLegacyNumber(legacyNumber);

	updateDeviceListStrings();

	orchestrator.setLogger(logger);

	const double warnTime = threadingMap.get(L"warnTime").asFloat(-1.0);
	const double killTimeout = std::clamp(threadingMap.get(L"killTimeout").asFloat(33.0), 0.01, 33.0);
	if (const auto threadingPolicy = threadingMap.get(L"policy").asIString(L"none");
		threadingPolicy == L"none") {
		useThreading = false;
	} else if (threadingPolicy == L"separateThread") {
		useThreading = true;
	} else {
		logger.error(L"Fatal error: Threading: unknown policy '{}'");
		return false;
	}

	orchestrator.setWarnTime(warnTime);
	orchestrator.setKillTimeout(killTimeout);

	updateTime = threadingMap.get(L"updateTime").asFloat(1.0 / 120.0);
	updateTime = std::clamp(updateTime, 1.0 / 200.0, 1.0);

	if (useThreading) {
		needToInitializeThread = true;
	}

	return true;
}

void ParentHelper::setParams(
	RequestedDeviceDescription request,
	ParamParser::ProcessingsInfoMap _patches
) {
	auto fullStateLock = getFullStateLock();

	optionsChanged = true;
	patches = std::move(_patches);
	requestedSource = std::move(request);

	if (needToInitializeThread) {
		needToInitializeThread = false;
		thread = std::thread{ [this]() { separateThreadFunction(); } };
	}
}

void ParentHelper::update(Snapshot& snap) {
	if (!useThreading) {
		pUpdate();
	}

	auto snapshotLock = getSnapshotLock();

	if (snapshotIsUpdated) {
		std::swap(snapshot.dataSnapshot, snap.dataSnapshot);
	} else {
		snap = snapshot;
		snapshotIsUpdated = true;
	}
}

void ParentHelper::separateThreadFunction() {
	using namespace std::chrono_literals;
	const auto sleepTime = 1.0s * updateTime;

	const auto res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	auto fullStateLock = getFullStateLock();

	if (res != S_OK) {
		logger.error(L"separate thread: CoInitializeEx failed");
		return;
	}

	while (true) {
		if (stopRequest.load()) {
			break;
		}

		pUpdate();

		if (stopRequest.load()) {
			break;
		}

		sleepVariable.wait_for(fullStateLock, sleepTime);
	}

	CoUninitialize();
}

void ParentHelper::pUpdate() {
	if (optionsChanged) {
		auto snapshotLock = getSnapshotLock();
		updateDevice();
		optionsChanged = false;
		// todo check why image in reset when I forgot to set optionsChanged = false
	}

	const auto changes = notificationClient.takeChanges();

	using DS = CaptureManager::DataSource;
	if (requestedSource.sourceType == DS::eDEFAULT_INPUT || requestedSource.sourceType == DS::eDEFAULT_OUTPUT) {
		const auto change =
			requestedSource.sourceType == DS::eDEFAULT_INPUT
				? changes.defaultCapture
				: changes.defaultRender;
		switch (change) {
			using DDC = utils::CMMNotificationClient::DefaultDeviceChange;
		case DDC::eNONE: break;
		case DDC::eCHANGED: {
			auto snapshotLock = getSnapshotLock();

			updateDevice();
			break;
		}
		case DDC::eNO_DEVICE: {
			const sview io = requestedSource.sourceType == DS::eDEFAULT_INPUT ? L"input" : L"output";
			logger.error(L"Requested default {} audio device, but all {} devices has been disconnected", io, io);
			auto snapshotLock = getSnapshotLock();
			deviceIsAvailable = false;
			snapshot.deviceIsAvailable = false;
			snapshotIsUpdated = false;
			break;
		}
		}
	}

	if (!changes.devices.empty()) {
		auto snapshotLock = getSnapshotLock();

		if (captureManager.getState() == CaptureManager::State::eDEVICE_CONNECTION_ERROR
			&& changes.devices.count(snapshot.diSnapshot.id) > 0) {
			updateDevice();
		}

		updateDeviceListStrings();
	}

	if (captureManager.getState() == CaptureManager::State::eDEVICE_IS_EXCLUSIVE) {
		captureManager.tryToRecoverFromExclusive();
	}

	if (captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		auto snapshotLock = getSnapshotLock();

		updateDevice();
	}

	if (captureManager.getState() != CaptureManager::State::eOK) {
		return;
	}

	const bool anyCaptured = captureManager.capture();
	if (anyCaptured) {
		orchestrator.process(captureManager.getChannelMixer());

		{
			auto snapshotLock = getSnapshotLock();
			orchestrator.exchangeData(snapshot.dataSnapshot);
		}
	}

	if (captureManager.getState() == CaptureManager::State::eRECONNECT_NEEDED) {
		updateDevice();
	}
}

void ParentHelper::updateDevice() {
	snapshotIsUpdated = false;

	captureManager.setSource(requestedSource.sourceType, requestedSource.id);

	deviceIsAvailable = captureManager.getState() == CaptureManager::State::eOK;
	if (!deviceIsAvailable) {
		return;
	}

	snapshot.deviceIsAvailable = deviceIsAvailable;

	// it's important that if device is not available
	// then #updateSnapshot is not called
	captureManager.updateSnapshot(snapshot.diSnapshot);

	orchestrator.patch(
		patches, legacyNumber,
		snapshot.diSnapshot.format.samplesPerSec,
		snapshot.diSnapshot.format.channelLayout
	);

	orchestrator.configureSnapshot(snapshot.dataSnapshot);
}

void ParentHelper::updateDeviceListStrings() {
	if (legacyNumber < 104) {
		snapshot.deviceListInput = enumerator.legacy_makeDeviceString(utils::MediaDeviceType::eINPUT);
		snapshot.deviceListOutput = enumerator.legacy_makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	} else {
		snapshot.deviceListInput = enumerator.makeDeviceString(utils::MediaDeviceType::eINPUT);
		snapshot.deviceListOutput = enumerator.makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	}
}

std::unique_lock<std::mutex> ParentHelper::getFullStateLock() {
	auto lock = std::unique_lock<std::mutex>{ fullStateMutex, std::defer_lock };
	if (useThreading) {
		lock.lock();
	}
	return lock;
}

std::unique_lock<std::mutex> ParentHelper::getSnapshotLock() {
	auto lock = std::unique_lock<std::mutex>{ snapshotMutex, std::defer_lock };
	if (useThreading) {
		lock.lock();
	}
	return lock;
}
