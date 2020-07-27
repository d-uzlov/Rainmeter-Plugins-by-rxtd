/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioEnumeratorHelper.h"

#include <string_view>

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

namespace rxtd::audio_analyzer {
	AudioEnumeratorHelper::AudioEnumeratorHelper(utils::Rainmeter::Logger& logger) :
		logger(logger) {

		if (!enumeratorWrapper.isValid()) {
			logger.error(L"Can't create device enumerator");
			valid = false;
			return;
		}

		updateDeviceLists();
	}

	bool AudioEnumeratorHelper::isValid() const {
		return valid;
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
				L"Can't get default {} audio device",
				type == utils::MediaDeviceType::eOUTPUT ? L"output" : L"input"
			);
			return std::nullopt;
		}

		return audioDeviceHandle;
	}

	string AudioEnumeratorHelper::getDefaultDeviceId(utils::MediaDeviceType type) {
		utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDefaultDevice(type);

		if (!audioDeviceHandle.isValid()) {
			return { };
		}

		return audioDeviceHandle.readDeviceId();
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

	const string& AudioEnumeratorHelper::getDeviceListInput() const {
		return deviceStringInput;
	}

	const string& AudioEnumeratorHelper::getDeviceListOutput() const {
		return deviceStringOutput;
	}

	void AudioEnumeratorHelper::updateDeviceStrings() {
		deviceStringInput = makeDeviceString(utils::MediaDeviceType::eINPUT);
		deviceStringOutput = makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	}

	string AudioEnumeratorHelper::makeDeviceString(utils::MediaDeviceType type) {
		auto collection = enumeratorWrapper.getActiveDevices(type);
		if (collection.empty()) {
			logger.debug(L"No {} devices found", type == utils::MediaDeviceType::eINPUT ? L"input" : L"output");
			return { };
		}

		string result;
		result.reserve(120 * collection.size());

		for (auto& device : collection) {
			const auto deviceInfo = device.readDeviceInfo();

			result += deviceInfo.id;
			result += L";"sv;
			result += deviceInfo.desc;
			result += L";"sv;
			result += deviceInfo.name;
			result += L"\n"sv;
		}

		result.resize(result.size() - 1);

		return result;
	}

	void AudioEnumeratorHelper::updateDeviceLists() {
		inputDevicesIDs = readDeviceList(utils::MediaDeviceType::eINPUT);
		outputDevicesIDs = readDeviceList(utils::MediaDeviceType::eOUTPUT);
	}

	std::set<string> AudioEnumeratorHelper::readDeviceList(utils::MediaDeviceType type) {
		auto collection = enumeratorWrapper.getAllDevices(type);
		if (collection.empty()) {
			logger.debug(L"No {} devices found", type == utils::MediaDeviceType::eINPUT ? L"input" : L"output");
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

		deviceStringLegacy.reserve(collection.size() * 120);

		for (auto& device : collection) {
			const auto deviceInfo = device.readDeviceInfo();

			deviceStringLegacy += deviceInfo.id;
			deviceStringLegacy += L" "sv;
			deviceStringLegacy += deviceInfo.fullFriendlyName;
			deviceStringLegacy += L"\n"sv;
		}

		deviceStringLegacy.resize(deviceStringLegacy.size());
	}
}
