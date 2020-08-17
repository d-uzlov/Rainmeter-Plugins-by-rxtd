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

AudioEnumeratorHelper::AudioEnumeratorHelper(utils::Rainmeter::Logger _logger) :
	logger(std::move(_logger)) {

	if (!enumeratorWrapper.isValid()) {
		logger.error(L"Can't create device enumerator");
		valid = false;
		return;
	}

	updateDeviceLists();
}

std::optional<utils::MediaDeviceWrapper> AudioEnumeratorHelper::getDevice(const string& deviceID) {
	auto typeOpt = getDeviceType(deviceID);
	if (!typeOpt.has_value()) {
		updateDeviceLists();
		typeOpt = getDeviceType(deviceID);
		if (!typeOpt.has_value()) {
			logger.error(L"Audio device is not found, id '{}'", deviceID);
			return std::nullopt;
		}
	}

	utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDeviceByID(typeOpt.value(), deviceID);

	if (!audioDeviceHandle.isValid()) {
		logger.error(L"Can't connect to audio device, id '{}'", deviceID);
		return std::nullopt;
	}

	return audioDeviceHandle;
}

std::optional<utils::MediaDeviceWrapper> AudioEnumeratorHelper::getDefaultDevice(utils::MediaDeviceType type) {
	utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDefaultDevice(type);

	if (!audioDeviceHandle.isValid()) {
		logger.error(
			L"Can't connect to default {} audio device",
			type == utils::MediaDeviceType::eOUTPUT ? L"output" : L"input"
		);
		return std::nullopt;
	}

	return audioDeviceHandle;
}

std::optional<utils::MediaDeviceType> AudioEnumeratorHelper::getDeviceType(const string& deviceID) {
	if (outputDevicesIDs.find(deviceID) != outputDevicesIDs.end()) {
		return utils::MediaDeviceType::eOUTPUT;
	}
	if (inputDevicesIDs.find(deviceID) != inputDevicesIDs.end()) {
		return utils::MediaDeviceType::eINPUT;
	}

	return std::nullopt;
}

void AudioEnumeratorHelper::updateDeviceStrings() {
	makeDeviceString(utils::MediaDeviceType::eINPUT, deviceStringInput);
	makeDeviceString(utils::MediaDeviceType::eOUTPUT, deviceStringOutput);
}

void AudioEnumeratorHelper::makeDeviceString(utils::MediaDeviceType type, string& result) {
	result.clear();

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return;
	}

	result.reserve(collection.size() * 100);

	for (auto& device : collection) {
		const auto deviceInfo = device.readDeviceInfo();

		result += deviceInfo.id;
		result += L";";
		result += deviceInfo.desc;
		result += L";";
		result += deviceInfo.name;
		result += L"\n";
	}

	result.resize(result.size() - 1);
}

void AudioEnumeratorHelper::updateDeviceLists() {
	inputDevicesIDs = readDeviceIdList(utils::MediaDeviceType::eINPUT);
	outputDevicesIDs = readDeviceIdList(utils::MediaDeviceType::eOUTPUT);
}

std::set<string> AudioEnumeratorHelper::readDeviceIdList(utils::MediaDeviceType type) {
	auto collection = enumeratorWrapper.getAllDevices(type);
	if (collection.empty()) {
		return { };
	}

	std::set<string> list;
	for (auto& device : collection) {
		list.insert(device.readDeviceId());
	}

	return list;
}

void AudioEnumeratorHelper::updateDeviceStringLegacy(utils::MediaDeviceType type) {
	deviceStringLegacy.clear();

	auto collection = enumeratorWrapper.getActiveDevices(type);
	if (collection.empty()) {
		return;
	}

	deviceStringLegacy.reserve(collection.size() * 100);

	for (auto& device : collection) {
		const auto deviceInfo = device.readDeviceInfo();

		deviceStringLegacy += deviceInfo.id;
		deviceStringLegacy += L" ";
		deviceStringLegacy += deviceInfo.fullFriendlyName;
		deviceStringLegacy += L"\n";
	}

	deviceStringLegacy.resize(deviceStringLegacy.size());
}
