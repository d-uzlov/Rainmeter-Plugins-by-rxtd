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
		state = State::eFATAL;
		return;
	}
}

DeviceManager::~DeviceManager() {
	deviceRelease();
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

	auto deviceOpt = deviceID.empty() ? enumerator.getDefaultDevice(source) : enumerator.getDevice(deviceID);

	if (!deviceOpt) {
		deviceRelease();
		return;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	captureManager = CaptureManager{ logger, *audioDeviceHandle.getPointer(), source == DataSource::eOUTPUT };

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			state = State::eFATAL;
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
	state = State::eERROR_AUTO;
}

void DeviceManager::readDeviceInfo() {
	deviceInfo = audioDeviceHandle.readDeviceInfo();
}

DeviceManager::State DeviceManager::getState() const {
	return state;
}

void DeviceManager::setOptions(DataSource source, sview deviceID) {
	// TODO this depends on function call order with deviceInit()

	if (this->deviceID == deviceID) {
		if (!deviceID.empty()) {
			return; // nothing has changed
		}

		// new ID is empty, new source is either input or output
		if (this->source == source) {
			return;
		}
	}

	state = State::eERROR_AUTO;

	this->deviceID = deviceID;
	this->source = determineDeviceType(source);

	deviceInit();
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
	if (state == State::eFATAL) {
		return;
	}

	if (!captureManager.isRecoverable() || !enumerator.isValid()) {
		state = State::eFATAL;
		deviceRelease();
		return;
	}

	if (!captureManager.isValid()) {
		state = State::eERROR_AUTO;
	}

	if (isDeviceChanged()) {
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

void DeviceManager::updateDeviceList() {
	enumerator.updateActiveDeviceList(source);
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

	const auto defaultDeviceId = enumerator.getDefaultDeviceId(source);

	return defaultDeviceId != deviceInfo.id;
}

DataSource DeviceManager::determineDeviceType(DataSource source) {
	if (source == DataSource::eINPUT || source == DataSource::eOUTPUT) {
		return source;
	}
	// eID

	const bool isOutputDevice = enumerator.isOutputDevice(deviceID);
	if (isOutputDevice) {
		return DataSource::eOUTPUT;
	}

	const bool isInputDevice = enumerator.isInputDevice(deviceID);
	if (isInputDevice) {
		return DataSource::eINPUT;
	}

	state = State::eERROR_MANUAL;
	logger.error(L"Device with id '{}' not found", deviceID);
	return DataSource::eINVALID;
}
