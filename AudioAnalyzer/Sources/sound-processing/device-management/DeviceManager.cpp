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

	std::optional<utils::MediaDeviceWrapper> deviceOpt;
	switch(source) {
	case DataSource::eDEFAULT_INPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
		break;

	case DataSource::eDEFAULT_OUTPUT: 
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
		break;

	case DataSource::eID:
		deviceOpt = enumerator.getDevice(deviceID);
		break;
	default: ;
	}

	if (!deviceOpt) {
		deviceRelease();
		return;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	captureManager = CaptureManager{ logger, audioDeviceHandle };

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

utils::MediaDeviceType DeviceManager::getCurrentDeviceType() const {
	return sourceType;
}

DeviceManager::State DeviceManager::getState() const {
	return state;
}

void DeviceManager::setOptions(DataSource source, sview deviceID) {
	// TODO this depends on function call order with deviceInit()

	if (this->deviceID == deviceID && this->source == source) {
		return;
	}

	state = State::eERROR_AUTO;

	this->deviceID = deviceID;
	this->source = source;
	sourceType = determineDeviceType(source, this->deviceID);

	deviceInit();
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

bool DeviceManager::isDeviceChanged() {
	if (source == DataSource::eID) {
		return false; // only default device can change
	}

	const auto defaultDeviceId = enumerator.getDefaultDeviceId(sourceType);

	return defaultDeviceId != deviceInfo.id;
}

utils::MediaDeviceType DeviceManager::determineDeviceType(DataSource source, const string& deviceID) {
	if (source == DataSource::eDEFAULT_INPUT) {
		return utils::MediaDeviceType::eINPUT;
	}
	if (source == DataSource::eDEFAULT_OUTPUT) {
		return utils::MediaDeviceType::eOUTPUT;
	}

	auto typeOpt = enumerator.getDeviceType(deviceID);
	if (!typeOpt.has_value()) {
		state = State::eERROR_MANUAL;
		logger.error(L"Device with id '{}' not found", deviceID);
		return { };
	}

	return typeOpt.value();
}
