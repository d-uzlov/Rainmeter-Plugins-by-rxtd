/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "DeviceManager.h"

#include <utility>

using namespace audio_analyzer;

DeviceManager::DeviceManager(Logger _logger, std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback)
	: enumerator(_logger), waveFormatUpdateCallback(std::move(waveFormatUpdateCallback)) {
	logger = std::move(_logger);
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
			logger.error(L"can't connect to default input device");
			state = State::eERROR_MANUAL;
			return;
		}
		break;

	case DataSource::eDEFAULT_OUTPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
		if (!deviceOpt) {
			logger.error(L"can't connect to default output device");
			state = State::eERROR_MANUAL;
			return;
		}
		break;

	case DataSource::eID:
		deviceOpt = enumerator.getDevice(requestedDevice.id);
		if (!deviceOpt) {
			logger.error(L"can't connect to specified device '{}'", requestedDevice.id);
			state = State::eERROR_MANUAL;
			return;
		}
		break;
	default: ;
	}

	audioDeviceHandle = std::move(deviceOpt.value());
	deviceInfo = audioDeviceHandle.readDeviceInfo();
	sourceDeviceType = audioDeviceHandle.getType();

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

	waveFormatUpdateCallback(captureManager.getWaveFormat());
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	audioDeviceHandle = { };
	deviceInfo = { };
	state = State::eERROR_AUTO;
}

utils::MediaDeviceType DeviceManager::getCurrentDeviceType() const {
	return sourceDeviceType;
}

DeviceManager::State DeviceManager::getState() const {
	return state;
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

const utils::MediaDeviceWrapper::DeviceInfo& DeviceManager::getDeviceInfo() const {
	return deviceInfo;
}

bool DeviceManager::getDeviceStatus() const {
	return audioDeviceHandle.isDeviceActive();
}

void DeviceManager::checkAndRepair() {
	if (state == State::eFATAL) {
		return;
	}

	if (!captureManager.isRecoverable()) {
		state = State::eFATAL;
		deviceRelease();
		return;
	}

	if (!captureManager.isValid()) {
		state = State::eERROR_AUTO;
	}

	if (state == State::eOK) {
		return;
	}

	if (clock::now() - lastDevicePollTime < DEVICE_POLL_TIMEOUT) {
		return; // not enough time has passed since last attempt
	}

	deviceInit();
}

CaptureManager& DeviceManager::getCaptureManager() {
	return captureManager;
}

const CaptureManager& DeviceManager::getCaptureManager() const {
	return captureManager;
}

AudioEnumeratorHelper& DeviceManager::getDeviceEnumerator() {
	return enumerator;
}

const AudioEnumeratorHelper& DeviceManager::getDeviceEnumerator() const {
	return enumerator;
}
