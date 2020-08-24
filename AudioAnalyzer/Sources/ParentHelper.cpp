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

bool ParentHelper::init(utils::Rainmeter::Logger _logger, index _legacyNumber, double computeTimeout, double killTimeout) {
	logger = std::move(_logger);
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
	orchestrator.setComputeTimeout(computeTimeout);
	orchestrator.setKillTimeout(killTimeout);

	return true;
}

void ParentHelper::setDevice(RequestedDeviceDescription request) {
	requestedSource = std::move(request);
	updateDevice();
}

void ParentHelper::patch(
	const ParamParser::ProcessingsInfoMap& patches,
	index legacyNumber,
	ProcessingOrchestrator::DataSnapshot& dataSnapshot
) {
	orchestrator.patch(patches, legacyNumber);
	orchestrator.configureSnapshot(snapshot.dataSnapshot);
	orchestrator.configureSnapshot(dataSnapshot);
}

bool ParentHelper::update() {
	const auto changes = notificationClient.getPointer()->takeChanges();

	// todo handle "no default device" and "device not found"

	if (const auto source = requestedSource.sourceType;
		source == DeviceManager::DataSource::eDEFAULT_INPUT && changes.defaultCapture
		|| source == DeviceManager::DataSource::eDEFAULT_OUTPUT && changes.defaultRender) {
		updateDevice();
	} else if (!changes.devices.empty()) {
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
		return false;

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
		orchestrator.exchangeData(snapshot.dataSnapshot);
		channelMixer.reset();
	}

	return true;
}

void ParentHelper::updateSnapshot(Snapshot& snap) {
	if (formatIsUpdated) {
		std::swap(snapshot.dataSnapshot, snap.dataSnapshot);
		return;
	}

	// todo check that dataSnapshot has newest info
	orchestrator.configureSnapshot(snap.dataSnapshot);

	snap.diSnapshot = snapshot.diSnapshot;
	snap.deviceListInput = snapshot.deviceListInput;
	snap.deviceListOutput = snapshot.deviceListOutput;
	snap.legacy_deviceList = snapshot.legacy_deviceList;

	formatIsUpdated = true;
}

void ParentHelper::updateDevice() {
	deviceManager.reconnect(enumerator, requestedSource.sourceType, requestedSource.id);
	deviceManager.updateDeviceInfoSnapshot(snapshot.diSnapshot);
	channelMixer.setFormat(snapshot.diSnapshot.format);
	orchestrator.setFormat(snapshot.diSnapshot.format.samplesPerSec, snapshot.diSnapshot.format.channelLayout);
	formatIsUpdated = false;
}
