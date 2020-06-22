/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "windows-wrappers/AudioBuffer.h"

#include "ParamParser.h"

#include "undef.h"
#include "CaseInsensitiveString.h"

using namespace audio_analyzer;

utils::ParentManager<AudioParent> AudioParent::parentManager{ };

AudioParent::AudioParent(utils::Rainmeter&& rain) :
	TypeHolder(std::move(rain)),
	deviceManager(logger, [this](auto format) { soundAnalyzer.setWaveFormat(format); }) {

	parentManager.add(*this);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

AudioParent::~AudioParent() {
	parentManager.remove(*this);
}

AudioParent* AudioParent::findInstance(utils::Rainmeter::Skin skin, isview measureName) {
	return parentManager.findParent(skin, measureName);
}

void AudioParent::_reload() {
	const auto source = this->rain.readString(L"Source") % ciView();
	string id = { };
	DataSource sourceEnum;
	if (source == L"" || source == L"Output") {
		sourceEnum = DataSource::eDEFAULT_OUTPUT;
	} else if (source == L"Input") {
		sourceEnum = DataSource::eDEFAULT_INPUT;
	} else {
		logger.debug(L"Using '{}' as source audio device ID.", source);
		sourceEnum = DataSource::eID;
		id = source % csView();
	}

	// legacy
	if (auto legacyID = this->rain.readString(L"DeviceID"); !legacyID.empty()) {
		logger.debug(L"Using '{}' as source audio device ID.", legacyID);
		sourceEnum = DataSource::eID;
		id = legacyID;

	} else if (const auto port = this->rain.readString(L"Port") % ciView(); !port.empty()) {
		if (port == L"Output") {
			sourceEnum = DataSource::eDEFAULT_OUTPUT;
		} else if (port == L"Input") {
			sourceEnum = DataSource::eDEFAULT_INPUT;
		} else {
			logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
			sourceEnum = DataSource::eDEFAULT_OUTPUT;
		}
	}

	deviceManager.setOptions(sourceEnum, id);


	auto targetRate = rain.read(L"TargetRate").asInt(44100);
	if (targetRate < 0) {
		logger.warning(L"Invalid TargetRate {}, must be > 0, assume 0.", targetRate);
		targetRate = 0;
	}

	soundAnalyzer.setTargetRate(targetRate);

	ParamParser paramParser(rain, rain.read(L"UnusedOptionsWarning").asBool(true));
	paramParser.parse();
	soundAnalyzer.setHandlerPatchers(paramParser.getHandlers(), paramParser.getPatches());
}

std::tuple<double, const wchar_t*> AudioParent::_update() {
	// TODO make an option for this value?
	constexpr index maxLoop = 15;

	deviceManager.checkAndRepair();
	if (deviceManager.getState() != DeviceManager::State::eOK) {
		if (deviceManager.getState() == DeviceManager::State::eFATAL) {
			setMeasureState(utils::MeasureState::eBROKEN);
			logger.error(L"Unrecoverable error");
		}
		soundAnalyzer.resetValues();
	} else {
		deviceManager.getCaptureManager().capture([&](bool silent, const uint8_t* buffer, uint32_t size) {
			soundAnalyzer.process(buffer, silent, size);
		}, maxLoop);
	}

	soundAnalyzer.finishStandalone();

	return std::make_tuple(deviceManager.getDeviceStatus(), nullptr);
}

void AudioParent::_command(const wchar_t* bangArgs) {
	const isview command = bangArgs;
	if (command == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(deviceManager.getCurrentDeviceType());
		deviceManager.getDeviceEnumerator().updateDeviceStrings();
		return;
	}

	logger.error(L"unknown command '{}'", command);
}

const wchar_t* AudioParent::_resolve(int argc, const wchar_t* argv[]) {
	if (argc < 1) {
		logger.error(L"Invalid section variable resolve: args needed", argc);
		return nullptr;
	}

	const isview optionName = argv[0];

	if (optionName == L"prop") {
		if (argc < 4) {
			logger.error(L"Invalid section variable resolve: need >= 4 argc, but only {} found", argc);
			return nullptr;
		}

		auto channelOpt = Channel::channelParser.find(argv[1]);
		if (!channelOpt.has_value()) {
			logger.error(L"Invalid section variable resolve: channel '{}' not recognized", argv[1]);
			return nullptr;
		}
		auto propVariant = soundAnalyzer.getAudioChildHelper().getProp(channelOpt.value(), argv[2], argv[3]);

		if (propVariant.index() == 1) {
			const auto error = std::get<1>(propVariant);
			switch (error) {
			case AudioChildHelper::SearchError::eCHANNEL_NOT_FOUND:
				logger.printer.print(L"Invalid section variable resolve: channel '{}' not found", argv[1]);
				return logger.printer.getBufferPtr();

			case AudioChildHelper::SearchError::eHANDLER_NOT_FOUND:
				logger.error(L"Invalid section variable resolve: handler '{}:{}' not found", argv[1], argv[2]);
				return nullptr;

			default:
				logger.error(L"Unexpected SearchError {}", error);
				return nullptr;
			}
		}
		if (propVariant.index() == 0) {
			const auto result = std::get<0>(propVariant);
			if (result == nullptr) {
				logger.error(L"Invalid section variable resolve: prop '{}:{}' not found", argv[2], argv[3]);
			}

			return result;
		}

		logger.error(L"Unexpected propVariant index {}", propVariant.index());
		return nullptr;
	}

	if (optionName == L"current device") {
		if (argc < 2) {
			logger.error(L"Invalid section variable resolve: need >= 2 argc, but only {} found", argc);
			return nullptr;
		}

		const isview deviceProperty = argv[1];

		if (deviceProperty == L"status") {
			return deviceManager.getDeviceStatus() ? L"1" : L"0";
		}
		if (deviceProperty == L"status string") {
			return deviceManager.getDeviceStatus() ? L"active" : L"down";
		}
		if (deviceProperty == L"type") {
			switch(deviceManager.getCurrentDeviceType()) {
			case utils::MediaDeviceType::eINPUT:
				return L"input";
			case utils::MediaDeviceType::eOUTPUT:
				return L"output";
			default: ;
			}
		}
		if (deviceProperty == L"name") {
			return deviceManager.getDeviceInfo().fullFriendlyName.c_str();
		}
		if (deviceProperty == L"nameOnly") {
			return deviceManager.getDeviceInfo().name.c_str();
		}
		if (deviceProperty == L"description") {
			return deviceManager.getDeviceInfo().desc.c_str();
		}
		if (deviceProperty == L"id") {
			return deviceManager.getDeviceInfo().id.c_str();
		}
		if (deviceProperty == L"format") {
			return deviceManager.getCaptureManager().getFormatString().c_str();
		}

		return nullptr;
	}


	if (optionName == L"device list") {
		return deviceManager.getDeviceEnumerator().getDeviceListLegacy().c_str();
	}
	if (optionName == L"device list input") {
		return deviceManager.getDeviceEnumerator().getDeviceListInput().c_str();
	}
	if (optionName == L"device list output") {
		return deviceManager.getDeviceEnumerator().getDeviceListOutput().c_str();
	}

	logger.error(L"Invalid section variable resolve: '{}' not supported", argv[0]);
	return nullptr;
}

double AudioParent::getValue(sview id, Channel channel, index index) const {
	return soundAnalyzer.getAudioChildHelper().getValue(channel, id % ciView(), index);
}
