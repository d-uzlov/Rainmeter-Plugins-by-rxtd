/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioEnumeratorHelper.h"

using namespace audio_analyzer;

std::optional<utils::MediaDeviceWrapper> AudioEnumeratorHelper::getDevice(const string& deviceID) {
	utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDeviceByID(deviceID);

	if (!audioDeviceHandle.isValid()) {
		return std::nullopt;
	}

	return audioDeviceHandle;
}

std::optional<utils::MediaDeviceWrapper> AudioEnumeratorHelper::getDefaultDevice(utils::MediaDeviceType type) {
	utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDefaultDevice(type);

	if (!audioDeviceHandle.isValid()) {
		return std::nullopt;
	}

	return audioDeviceHandle;
}

string AudioEnumeratorHelper::makeDeviceListString(utils::MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;
	}

	utils::BufferPrinter bp;
	for (auto& device : collection) {
		const auto deviceInfo = device.readDeviceInfo();
		bp.append(L"{};{};{}\n", deviceInfo.id, deviceInfo.desc, deviceInfo.name);
	}

	result = bp.getBufferView();
	result.pop_back(); // removes \0
	result.pop_back(); // removes \n

	return result;
}

string AudioEnumeratorHelper::legacy_makeDeviceListString(utils::MediaDeviceType type) {
	string result;

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return result;;
	}

	utils::BufferPrinter bp;
	for (auto& device : collection) {
		const auto deviceInfo = device.readDeviceInfo();

		bp.append(L"{} {}\n", deviceInfo.id, deviceInfo.fullFriendlyName);
	}

	result = bp.getBufferView();
	result.pop_back();
	result.pop_back();

	return result;
}
