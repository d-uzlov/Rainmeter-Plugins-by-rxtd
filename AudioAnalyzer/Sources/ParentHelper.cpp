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

	deviceManager.setLogger(logger);
	deviceManager.setLegacyNumber(legacyNumber);

	notificationClient = {
		[=](auto ptr) {
			*ptr = new utils::CMMNotificationClient{ enumerator.getWrapper() };
			return true;
		}
	};

	enumerator.updateDeviceStrings();
	enumerator.updateDeviceStringLegacy(snapshot.diSnapshot.type);
	snapshot.deviceListInput = enumerator.getDeviceListInput();
	snapshot.deviceListOutput = enumerator.getDeviceListOutput();
	snapshot.legacy_deviceList = enumerator.legacy_getDeviceList();

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
	const ParamParser::ProcessingsInfoMap& patches,
	index legacyNumber,
	Snapshot& snap
) {
	auto fullStateLock = getFullStateLock();
	// snapshot lock is not needed
	// because separate thread either sleeps
	// or is guarded by fullStateLock

	orchestrator.patch(patches, legacyNumber);

	requestedSource = std::move(request);
	updateDevice();
	fullSnapshotUpdate(snap);

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
		fullSnapshotUpdate(snap);
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
	const auto changes = notificationClient.getPointer()->takeChanges();

	// todo handle "no default device" and "device not found"

	if (const auto source = requestedSource.sourceType;
		source == DeviceManager::DataSource::eDEFAULT_INPUT && changes.defaultCapture
		|| source == DeviceManager::DataSource::eDEFAULT_OUTPUT && changes.defaultRender) {
		auto snapshotLock = getSnapshotLock();

		updateDevice();
	} else if (!changes.devices.empty()) {
		auto snapshotLock = getSnapshotLock();

		if (changes.devices.count(snapshot.diSnapshot.id) > 0) {
			updateDevice();
		}

		enumerator.updateDeviceStrings();
		enumerator.updateDeviceStringLegacy(snapshot.diSnapshot.type);
		snapshot.deviceListInput = enumerator.getDeviceListInput();
		snapshot.deviceListOutput = enumerator.getDeviceListOutput();
		snapshot.legacy_deviceList = enumerator.legacy_getDeviceList();
	}

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		auto snapshotLock = getSnapshotLock();
		snapshot.fatalError = true;
		return;

		// todo
		// for (auto& [name, sa] : saMap) {
		// 	sa.resetValues();
		// }
	}

	bool any = false;
	deviceManager.getCaptureManager().capture([&](utils::array2d_view<float> channelsData) {
		channelMixer.saveChannelsData(channelsData, true);
		any = true;
	});

	if (any) {
		orchestrator.process(channelMixer);
		channelMixer.reset();

		{
			auto snapshotLock = getSnapshotLock();
			orchestrator.exchangeData(snapshot.dataSnapshot);
		}
	}
}

void ParentHelper::updateDevice() {
	deviceManager.reconnect(enumerator, requestedSource.sourceType, requestedSource.id);

	deviceManager.updateDeviceInfoSnapshot(snapshot.diSnapshot);

	channelMixer.setFormat(snapshot.diSnapshot.format);
	orchestrator.setFormat(snapshot.diSnapshot.format.samplesPerSec, snapshot.diSnapshot.format.channelLayout);

	orchestrator.configureSnapshot(snapshot.dataSnapshot);

	snapshotIsUpdated = false;
}

void ParentHelper::fullSnapshotUpdate(Snapshot& snap) const {
	orchestrator.configureSnapshot(snap.dataSnapshot);

	snap.diSnapshot = snapshot.diSnapshot;
	snap.deviceListInput = snapshot.deviceListInput;
	snap.deviceListOutput = snapshot.deviceListOutput;
	snap.legacy_deviceList = snapshot.legacy_deviceList;
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
