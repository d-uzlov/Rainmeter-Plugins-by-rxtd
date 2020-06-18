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

#include "windows-wrappers/GenericComWrapper.h"

#include "undef.h"


using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;


DeviceManager::DeviceManager(utils::Rainmeter::Logger& logger, std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback)
	: logger(logger), enumerator(logger), waveFormatUpdateCallback(std::move(waveFormatUpdateCallback)) {
	if (!enumerator.isValid()) {
		recoverable = false;
		return;
	}
}

DeviceManager::~DeviceManager() {
	deviceRelease();
}

void DeviceManager::deviceInit() {
	if (!isRecoverable()) {
		return;
	}

	deviceRelease();

	lastDevicePollTime = clock::now();
	valid = true;

	auto deviceOpt = deviceID.empty() ? enumerator.getDefaultDevice(port) : enumerator.getDevice(deviceID, port);

	if (!deviceOpt) {
		deviceRelease();
		return;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	captureManager = CaptureManager{ logger, *audioDeviceHandle.getPointer(), port == Port::eOUTPUT };

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			recoverable = false;
		}

		deviceRelease();
		return;
	}

	readDeviceInfo();

	waveFormatUpdateCallback(captureManager.getWaveFormat());
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	audioDeviceHandle = { };
	deviceInfo = { };
	valid = false;
}

void DeviceManager::readDeviceInfo() {
	deviceInfo = audioDeviceHandle.readDeviceInfo();
}

bool DeviceManager::isValid() const {
	return valid;
}

bool DeviceManager::isRecoverable() const {
	return recoverable;
}

void DeviceManager::setOptions(Port port, sview deviceID) {
	// TODO this depends on function call order with deviceInit()

	this->port = port;
	this->deviceID = deviceID;
}

CaptureManager::BufferFetchResult DeviceManager::nextBuffer() {
	return captureManager.nextBuffer();
}

const utils::MediaDeviceWrapper::DeviceInfo& DeviceManager::getDeviceInfo() const {
	return deviceInfo;
}

bool DeviceManager::getDeviceStatus() const {
	return audioDeviceHandle.isDeviceActive();
}

void DeviceManager::checkAndRepair() {
	if (!isRecoverable()) {
		return;
	}

	if (!captureManager.isRecoverable() || !enumerator.isValid()) {
		recoverable = false;
		deviceRelease();
		return;
	}

	if (!captureManager.isValid()) {
		valid = false;
	}

	if (isDeviceChanged()) {
		valid = false;
	}

	if (isValid()) {
		return;
	}

	if (clock::now() - lastDevicePollTime < DEVICE_POLL_TIMEOUT) {
		return; // not enough time has passed since last attempt
	}

	deviceInit();
}

void DeviceManager::updateDeviceList() {
	enumerator.updateDeviceList(port);
}

const CaptureManager& DeviceManager::getCaptureManager() const {
	return captureManager;
}

const AudioEnumeratorWrapper& DeviceManager::getDeviceEnumerator() const {
	return enumerator;
}

bool DeviceManager::isDeviceChanged() {
	if (!deviceID.empty()) {
		return false; // only default device can change
	}

	const auto defaultDeviceId = enumerator.getDefaultDeviceId(port);

	return defaultDeviceId != deviceInfo.id;
}
