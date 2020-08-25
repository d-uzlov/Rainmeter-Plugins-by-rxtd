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
	utils::Rainmeter::Logger logger,
	utils::OptionMap threadingMap,
	index _legacyNumber
) {
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
	ParamParser::ProcessingsInfoMap _patches,
	Snapshot& snap
) {
	auto fullStateLock = getFullStateLock();
	// snapshot lock is not needed
	// because separate thread either sleeps
	// or is guarded by fullStateLock

	patches = std::move(_patches);
	requestedSource = std::move(request);
	updateDevice();

	snap = snapshot;
	snapshotIsUpdated = true;

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

	snap.fatalError = snapshot.fatalError;
}

void ParentHelper::separateThreadFunction() {
	using namespace std::chrono_literals;
	const auto sleepTime = 1.0s * updateTime;

	auto fullStateLock = getFullStateLock();

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
}

void ParentHelper::pUpdate() {
	const auto changes = notificationClient.takeChanges();

	bool deviceUpdated = false;

	using DS = CaptureManager::DataSource;
	if (requestedSource.sourceType == DS::eDEFAULT_INPUT || requestedSource.sourceType == DS::eDEFAULT_OUTPUT) {
		const auto change = requestedSource.sourceType == DS::eDEFAULT_INPUT
			                    ? changes.defaultCapture
			                    : changes.defaultRender;
		switch (change) {
			using DDC = utils::CMMNotificationClient::DefaultDeviceChange;
		case DDC::eNONE: break;
		case DDC::eCHANGED: {
			auto snapshotLock = getSnapshotLock();

			deviceUpdated = true;
			updateDevice();
			break;
		}
		case DDC::eNO_DEVICE: {
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

		if (!deviceUpdated && changes.devices.count(snapshot.diSnapshot.id) > 0) {
			updateDevice();
		}

		updateDeviceListStrings();
	}
	if (!deviceUpdated && !captureManager.isValid()) {
		auto snapshotLock = getSnapshotLock();
		updateDevice();
	}

	if (!deviceIsAvailable) {
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
}

void ParentHelper::updateDevice() {
	snapshotIsUpdated = false;

	const bool ok = captureManager.setSource(requestedSource.sourceType, requestedSource.id);
	if (!ok) {
		snapshot.fatalError = true;
		deviceIsAvailable = false;
		return;
	}

	deviceIsAvailable = captureManager.isValid();
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
