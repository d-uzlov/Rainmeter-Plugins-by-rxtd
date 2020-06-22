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

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

namespace rxtd::audio_analyzer {

	AudioEnumeratorHelper::AudioEnumeratorHelper(utils::Rainmeter::Logger& logger) :
		logger(logger) {

		if (!enumeratorWrapper.isValid()) {
			logger.error(L"Can't create device enumerator, error {error}", enumeratorWrapper.getLastResult());
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
				logger.error(L"Audio device with ID '{}' not found", deviceID);
				return std::nullopt;
			}
		}

		utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDeviceByID(typeOpt.value(), deviceID);

		if (enumeratorWrapper.getLastResult() != S_OK) {
			logger.error(L"Audio device with ID '{}' not found, error code {error}", deviceID, enumeratorWrapper.getLastResult());
			return std::nullopt;
		}

		return audioDeviceHandle;
	}

	std::optional<utils::MediaDeviceWrapper> AudioEnumeratorHelper::getDefaultDevice(utils::MediaDeviceType type) {
		utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDefaultDevice(type);

		if (enumeratorWrapper.getLastResult() != S_OK) {
			logger.error(L"Can't get default {} audio device, error code {error}", type == utils::MediaDeviceType::eOUTPUT ? L"output" : L"input", enumeratorWrapper.getLastResult());
			valid = false;
			return std::nullopt;
		}

		return audioDeviceHandle;
	}

	string AudioEnumeratorHelper::getDefaultDeviceId(utils::MediaDeviceType type) {
		utils::MediaDeviceWrapper audioDeviceHandle = enumeratorWrapper.getDefaultDevice(type);

		if (enumeratorWrapper.getLastResult() != S_OK) {
			valid = false;
			return { };
		}

		return audioDeviceHandle.readDeviceId();
	}

	std::optional<utils::MediaDeviceType> AudioEnumeratorHelper::getDeviceType(const string& deviceID) {
		if (outputDevices.find(deviceID) != outputDevices.end()) {
			return utils::MediaDeviceType::eOUTPUT;
		}
		if (inputDevices.find(deviceID) != inputDevices.end()) {
			return utils::MediaDeviceType::eINPUT;
		}

		return std::nullopt;
	}

	const string& AudioEnumeratorHelper::getDeviceListLegacy() const {
		return deviceStringLegacy;
	}

	const string& AudioEnumeratorHelper::getDeviceListInput() const {
		return deviceStringInput;
	}

	const string& AudioEnumeratorHelper::getDeviceListOutput() const {
		return deviceStringOutput;
	}

	void AudioEnumeratorHelper::updateDeviceStringLegacy(utils::MediaDeviceType type) {
		deviceStringLegacy.clear();

		auto collection = enumeratorWrapper.enumerateActiveDevices(type);
		if (collection.getLastResult() != S_OK) {
			logger.error(L"Can't read audio device list: EnumAudioEndpoints() failed, error code {}", collection.getLastResult());
			valid = false;
			return;
		}

		const auto devicesCount = collection.getDevicesCount();

		deviceStringLegacy.reserve(devicesCount * 120);

		for (index deviceIndex = 0; deviceIndex < devicesCount; ++deviceIndex) {
			utils::MediaDeviceWrapper device  = collection.get(deviceIndex);

			const auto deviceInfo = device.readDeviceInfo();

			deviceStringLegacy += deviceInfo.id;
			deviceStringLegacy += L" "sv;
			deviceStringLegacy += deviceInfo.fullFriendlyName;
			deviceStringLegacy += L"\n"sv;
		}

		deviceStringLegacy.resize(deviceStringLegacy.size() - 1);
	}

	void AudioEnumeratorHelper::updateDeviceStrings() {
		deviceStringInput = makeDeviceString(utils::MediaDeviceType::eINPUT);
		deviceStringOutput = makeDeviceString(utils::MediaDeviceType::eOUTPUT);
	}

	string AudioEnumeratorHelper::makeDeviceString(utils::MediaDeviceType type) {
		auto collection = enumeratorWrapper.enumerateActiveDevices(type);
		if (collection.getLastResult() != S_OK) {
			logger.error(L"Can't read audio device list: EnumAudioEndpoints() failed, error code {}", collection.getLastResult());
			valid = false;
			return { };
		}

		const auto devicesCount = collection.getDevicesCount();

		string result;
		result.reserve(devicesCount * 120);

		for (index deviceIndex = 0; deviceIndex < devicesCount; ++deviceIndex) {
			utils::MediaDeviceWrapper device = collection.get(deviceIndex);

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
		inputDevices = readDeviceList(utils::MediaDeviceType::eINPUT);
		outputDevices = readDeviceList(utils::MediaDeviceType::eOUTPUT);
	}

	std::set<string> AudioEnumeratorHelper::readDeviceList(utils::MediaDeviceType type) {
		auto collection = enumeratorWrapper.enumerateAllDevices(type);
		if (collection.getLastResult() != S_OK) {
			logger.error(L"Can't read audio device list: EnumAudioEndpoints() failed, error code {}", collection.getLastResult());
			valid = false;
			return { };
		}

		const auto devicesCount = collection.getDevicesCount();

		std::set<string> list;

		for (index deviceIndex = 0; deviceIndex < devicesCount; ++deviceIndex) {
			utils::MediaDeviceWrapper device = collection.get(deviceIndex);

			const auto id = device.readDeviceId();

			list.insert(id);
		}

		return list;
	}
}
