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

bool DeviceManager::reconnect(AudioEnumeratorHelper& enumerator, DataSource type, const string& id) {
	deviceRelease();

	state = State::eOK;

	std::optional<utils::MediaDeviceWrapper> deviceOpt = getDevice(enumerator, type, id);

	if (!deviceOpt) {
		state = State::eERROR_MANUAL;
		return true;
	}

	utils::MediaDeviceWrapper audioDeviceHandle = std::move(deviceOpt.value());

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	diSnapshot.status = true;
	diSnapshot.id = deviceInfo.id;
	diSnapshot.description = deviceInfo.desc;
	diSnapshot.name = legacyNumber < 104 ? deviceInfo.fullFriendlyName : deviceInfo.name;
	diSnapshot.type = audioDeviceHandle.getType();

	captureManager = CaptureManager{ logger, std::move(audioDeviceHandle) };
	diSnapshot.format = captureManager.getWaveFormat();
	diSnapshot.formatString = captureManager.getFormatString();

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			state = State::eFATAL;
			logger.debug(L"device manager fatal");
			return false;
		}

		logger.debug(L"device manager recoverable, but won't try this time");

		deviceRelease();
		return true;
	}

	return true;
}

std::optional<utils::MediaDeviceWrapper>
DeviceManager::getDevice(AudioEnumeratorHelper& enumerator, DataSource type, const string& id) {
	switch (type) {
	case DataSource::eDEFAULT_INPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
	case DataSource::eDEFAULT_OUTPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
	case DataSource::eID:
		return enumerator.getDevice(id);
	}

	return { };
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	diSnapshot = { };
}
