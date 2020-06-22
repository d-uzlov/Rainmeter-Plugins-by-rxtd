/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioEnumeratorWrapper.h"

#include <string_view>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

namespace rxtd::audio_analyzer {

	AudioEnumeratorWrapper::AudioEnumeratorWrapper(utils::Rainmeter::Logger& logger) :
		logger(logger) {

		HRESULT res = CoCreateInstance(
			CLSID_MMDeviceEnumerator,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_IMMDeviceEnumerator,
			reinterpret_cast<void**>(&audioEnumeratorHandle)
		);

		if (res != S_OK) {
			audioEnumeratorHandle.release();
			logger.error(L"Can't create device enumerator, error {error}", res);
			valid = false;
			return;
		}

		updateDeviceLists();
	}

	bool AudioEnumeratorWrapper::isValid() const {
		return valid;
	}

	std::optional<utils::MediaDeviceWrapper> AudioEnumeratorWrapper::getDevice(const string& deviceID) {
		auto typeOpt = getDeviceType(deviceID);
		if (!typeOpt.has_value()) {
			updateDeviceLists();
			typeOpt = getDeviceType(deviceID);
			if (!typeOpt.has_value()) {
				logger.error(L"Audio device with ID '{}' not found", deviceID);
				return std::nullopt;
			}
		}

		utils::MediaDeviceWrapper audioDeviceHandle { typeOpt.value() };

		HRESULT resultCode = audioEnumeratorHandle->GetDevice(deviceID.c_str(), &audioDeviceHandle);
		if (resultCode != S_OK) {
			logger.error(L"Audio device with ID '{}' not found, error code {error}", deviceID, resultCode);
			return std::nullopt;
		}

		return audioDeviceHandle;
	}

	std::optional<utils::MediaDeviceWrapper> AudioEnumeratorWrapper::getDefaultDevice(utils::MediaDeviceType port) {
		utils::MediaDeviceWrapper audioDeviceHandle { port };

		HRESULT resultCode = audioEnumeratorHandle->GetDefaultAudioEndpoint(port == utils::MediaDeviceType::eOUTPUT ? eRender : eCapture, eConsole, &audioDeviceHandle);
		if (resultCode != S_OK) {
			logger.error(L"Can't get default {} audio device, error code {error}", port == utils::MediaDeviceType::eOUTPUT ? L"output" : L"input", resultCode);
			valid = false;
			return std::nullopt;
		}

		return audioDeviceHandle;
	}

	string AudioEnumeratorWrapper::getDefaultDeviceId(utils::MediaDeviceType port) {
		utils::MediaDeviceWrapper audioDeviceHandle { port };

		HRESULT resultCode = audioEnumeratorHandle->GetDefaultAudioEndpoint(port == utils::MediaDeviceType::eOUTPUT ? eRender : eCapture, eConsole, &audioDeviceHandle);

		if (resultCode != S_OK) {
			valid = false;
			return { };
		}

		return audioDeviceHandle.readDeviceId();
	}

	std::optional<utils::MediaDeviceType> AudioEnumeratorWrapper::getDeviceType(const string& deviceID) {
		if (outputDevices.find(deviceID) != outputDevices.end()) {
			return utils::MediaDeviceType::eOUTPUT;
		}
		if (inputDevices.find(deviceID) != inputDevices.end()) {
			return utils::MediaDeviceType::eINPUT;
		}

		return std::nullopt;
	}

	const string& AudioEnumeratorWrapper::getDeviceListLegacy() const {
		return deviceListLegacy;
	}

	const string& AudioEnumeratorWrapper::getActiveDeviceList() const {
		return deviceListActive;
	}

	void AudioEnumeratorWrapper::updateActiveDeviceList(utils::MediaDeviceType port) {
		deviceListLegacy.clear();
		deviceListActive.clear();

		utils::GenericComWrapper<IMMDeviceCollection> collection;
		const auto collectionOpeningState = audioEnumeratorHandle->EnumAudioEndpoints(port == utils::MediaDeviceType::eOUTPUT ? eRender : eCapture, DEVICE_STATE_ACTIVE, &collection);
		if (collectionOpeningState != S_OK) {
			logger.error(L"Can't read audio device list: EnumAudioEndpoints() failed, error code {}", collectionOpeningState);
			valid = false;
			return;
		}

		static_assert(std::is_same<UINT, uint32_t>::value);
		uint32_t devicesCount;
		collection->GetCount(&devicesCount);

		deviceListLegacy.reserve(devicesCount * 120);
		deviceListActive.reserve(devicesCount * 120);

		for (index deviceIndex = 0; deviceIndex < index(devicesCount); ++deviceIndex) {
			utils::MediaDeviceWrapper device { port };
			if (collection->Item(UINT(deviceIndex), &device) != S_OK) {
				continue;
			}

			const auto deviceInfo = device.readDeviceInfo();

			deviceListLegacy += deviceInfo.id;
			deviceListLegacy += L" "sv;
			deviceListLegacy += deviceInfo.fullFriendlyName;
			deviceListLegacy += L"\n"sv;

			deviceListActive += deviceInfo.id;
			deviceListActive += L";"sv;
			deviceListActive += deviceInfo.desc;
			deviceListActive += L";"sv;
			deviceListActive += deviceInfo.name;
			deviceListActive += L"\n"sv;
		}

		deviceListLegacy.resize(deviceListLegacy.size() - 1);
		deviceListActive.resize(deviceListActive.size() - 1);
	}

	void AudioEnumeratorWrapper::updateDeviceLists() {
		inputDevices = readDeviceList(utils::MediaDeviceType::eINPUT);
		outputDevices = readDeviceList(utils::MediaDeviceType::eOUTPUT);
	}

	std::set<string> AudioEnumeratorWrapper::readDeviceList(utils::MediaDeviceType port) {
		utils::GenericComWrapper<IMMDeviceCollection> collection;
		const auto collectionOpeningState = audioEnumeratorHandle->EnumAudioEndpoints(port == utils::MediaDeviceType::eOUTPUT ? eRender : eCapture, DEVICE_STATEMASK_ALL, &collection);
		if (collectionOpeningState != S_OK) {
			logger.error(L"Can't read audio device list: EnumAudioEndpoints() failed, error code {}", collectionOpeningState);
			valid = false;
			return { };
		}

		static_assert(std::is_same<UINT, uint32_t>::value);
		uint32_t devicesCount;
		collection->GetCount(&devicesCount);

		std::set<string> list;

		for (index deviceIndex = 0; deviceIndex < index(devicesCount); ++deviceIndex) {
			utils::MediaDeviceWrapper device { port };
			if (collection->Item(UINT(deviceIndex), &device) != S_OK) {
				continue;
			}

			const auto id = device.readDeviceId();
			list.insert(id);
		}

		return list;
	}
}
