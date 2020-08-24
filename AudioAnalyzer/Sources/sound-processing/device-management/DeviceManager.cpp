/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "DeviceManager.h"

using namespace audio_analyzer;

DeviceManager::DeviceManager(std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback):
	waveFormatUpdateCallback(std::move(waveFormatUpdateCallback)) {
	if (!enumerator.isValid()) {
		state = State::eFATAL;
		return;
	}
}

void DeviceManager::deviceInit() {
	if (getState() != State::eERROR_AUTO) {
		// eOK - init not needed
		// eERROR_MANUAL - change settings before calling this function
		return;
	}

	deviceRelease();

	lastDevicePollTime = clock::now();
	state = State::eOK;

	std::optional<utils::MediaDeviceWrapper> deviceOpt;
	switch (requestedDevice.type) {
	case DataSource::eDEFAULT_INPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return;
		}
		break;

	case DataSource::eDEFAULT_OUTPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return;
		}
		break;

	case DataSource::eID:
		deviceOpt = enumerator.getDevice(requestedDevice.id);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return;
		}
		break;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	captureManager = CaptureManager{ logger, audioDeviceHandle };

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			state = State::eFATAL;
			logger.debug(L"device manager fatal");
		} else {
			logger.debug(L"device manager recoverable, but won't try this time");
		}

		deviceRelease();
		return;
	}

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	diSnapshot.status = true;
	diSnapshot.id = deviceInfo.id;
	diSnapshot.description = deviceInfo.desc;
	diSnapshot.name = legacyNumber < 104 ? deviceInfo.fullFriendlyName : deviceInfo.name;
	diSnapshot.format = captureManager.getFormatString();
	diSnapshot.type = audioDeviceHandle.getType();

	waveFormatUpdateCallback(captureManager.getWaveFormat());
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	audioDeviceHandle = { };
	diSnapshot = { };
	state = State::eERROR_AUTO;
}

void DeviceManager::forceReconnect() {
	state = State::eERROR_AUTO;
	deviceInit();
}

void DeviceManager::setOptions(DataSource source, sview deviceID) {
	if (requestedDevice.id == deviceID && requestedDevice.type == source) {
		return;
	}

	requestedDevice.id = deviceID;
	requestedDevice.type = source;

	forceReconnect();
}
