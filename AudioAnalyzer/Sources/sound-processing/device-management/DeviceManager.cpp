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
	: logger(logger), waveFormatUpdateCallback(std::move(waveFormatUpdateCallback)) {
}

DeviceManager::~DeviceManager() {
	deviceRelease();
}

void DeviceManager::deviceInit(AudioEnumeratorWrapper &enumerator) {
	lastDevicePollTime = clock::now();

	const bool handleAcquired = acquireDeviceHandle(enumerator);
	if (!handleAcquired) {
		deviceRelease();
		return;
	}

	captureManager = CaptureManager{ logger, *audioDeviceHandle.getPointer(), port == Port::eOUTPUT };

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			objectIsValid = false;
		}
		deviceRelease();
		return;
	}

	if (!captureManager.isValid()) {
		logger.error(L"Can't create capture client");
		deviceRelease();
		return;
	}

	readDeviceInfo();

	waveFormatUpdateCallback(captureManager.getWaveFormat());
}

bool DeviceManager::acquireDeviceHandle(AudioEnumeratorWrapper &enumerator) {
	// if no ID specified, get default audio device
	if (deviceID.empty()) {
		auto deviceOpt = enumerator.getDefaultDevice(port);
	
		if (!deviceOpt) {
			return false;
		}

		audioDeviceHandle = std::move(deviceOpt.value());

		return true;
	}

	auto deviceOpt = enumerator.getDevice(deviceID, port);
	
	if (!deviceOpt) {
		return false;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	return true;
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	audioDeviceHandle = { };
	deviceInfo = { };
}

void DeviceManager::readDeviceInfo() {
	deviceInfo = audioDeviceHandle.readDeviceInfo();
}

void DeviceManager::ensureDeviceAcquired(AudioEnumeratorWrapper &enumerator) {
	// if current state is invalid, then we must reacquire the handles, but only if enough time has passed since previous attempt

	if (captureManager.isValid()) {
		return;
	}

	if (clock::now() - lastDevicePollTime < DEVICE_POLL_TIMEOUT) {
		return; // not enough time has passed
	}

	deviceInit(enumerator);

	// TODO this is incorrect: if there was an error, we will still proceed polling buffers
}

bool DeviceManager::isObjectValid() const {
	return objectIsValid;
}

void DeviceManager::setOptions(Port port, sview deviceID) {
	if (!objectIsValid) {
		return;
	}

	this->port = port;
	this->deviceID = deviceID;
}

void DeviceManager::init(AudioEnumeratorWrapper &enumerator) {
	if (!objectIsValid) {
		return;
	}

	deviceRelease();
	deviceInit(enumerator);
}

bool DeviceManager::actualizeDevice(AudioEnumeratorWrapper &enumerator) {
	if (!deviceID.empty()) {
		return false; // nothing to actualize, only default device can change
	}

	const auto defaultDeviceId = enumerator.getDefaultDeviceId(port);

	if (defaultDeviceId != deviceInfo.id) {
		deviceRelease();
		deviceInit(enumerator);
		return true;
	}

	return false;
}

CaptureManager::BufferFetchResult DeviceManager::nextBuffer(AudioEnumeratorWrapper &enumerator) {
	if (!objectIsValid) {
		return CaptureManager::BufferFetchResult::invalidState();
	}

	ensureDeviceAcquired(enumerator);

	return captureManager.nextBuffer();
}


const string& DeviceManager::getDeviceName() const {
	return deviceInfo.name;
}

const string& DeviceManager::getDeviceId() const {
	return deviceInfo.id;
}

bool DeviceManager::getDeviceStatus() const {
	return audioDeviceHandle.isDeviceActive();
}

const string& DeviceManager::getDeviceFormat() const {
	return captureManager.getFormatString();
}

Port DeviceManager::getPort() const {
	return port;
}
