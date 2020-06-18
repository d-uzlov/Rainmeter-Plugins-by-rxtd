﻿/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "windows-wrappers/BufferWrapper.h"

#include "ParamParser.h"

#include "undef.h"
#include "CaseInsensitiveString.h"

using namespace audio_analyzer;

utils::ParentManager<AudioParent> AudioParent::parentManager{ };

AudioParent::AudioParent(utils::Rainmeter&& rain) :
	TypeHolder(std::move(rain)),
	deviceManager(log, [this](auto format) { soundAnalyzer.setWaveFormat(format); }) {

	parentManager.add(*this);

	if (!deviceManager.isRecoverable()) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	// parse port specifier
	const auto port = this->rain.readString(L"Port") % ciView();
	Port portEnum;
	if (port == L"" || port == L"Output") {
		portEnum = Port::eOUTPUT;
	} else if (port == L"Input") {
		portEnum = Port::eINPUT;
	} else {
		log.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
		portEnum = Port::eOUTPUT;
	}

	auto id = this->rain.readString(L"DeviceID");

	deviceManager.setOptions(portEnum, id);
	deviceManager.deviceInit();
}

AudioParent::~AudioParent() {
	parentManager.remove(*this);
}

AudioParent* AudioParent::findInstance(utils::Rainmeter::Skin skin, isview measureName) {
	return parentManager.findParent(skin, measureName);
}

void AudioParent::_reload() {
	ParamParser paramParser(rain, rain.readBool(L"UnusedOptionsWarning", true));
	paramParser.parse();

	auto rateLimit = std::max<index>(rain.readInt(L"TargetRate"), 0);
	if (rateLimit < 0) {
		rateLimit = 0;
	}
	if (rateLimit > 0 && rateLimit < 8000) {
		rateLimit = 8000;
	}

	soundAnalyzer.setTargetRate(rateLimit);

	soundAnalyzer.setPatchHandlers(paramParser.getHandlers(), paramParser.getPatches());
}

std::tuple<double, const wchar_t*> AudioParent::_update() {
	// TODO make an option for this value?
	constexpr index maxBuffers = 15;

	bool changed = deviceManager.actualizeDevice();

	for (index i = 0; i < maxBuffers; ++i) {
		const auto fetchResult = deviceManager.nextBuffer();
		switch (fetchResult.getState()) {
		case CaptureManager::BufferFetchState::eOK: {
			auto& bufferWrapper = fetchResult.getBuffer();

			const uint8_t*const buffer = bufferWrapper.getBuffer();
			const uint32_t framesCount = bufferWrapper.getFramesCount();

			soundAnalyzer.process(buffer, bufferWrapper.isSilent(), framesCount);
			break;
		}

		case CaptureManager::BufferFetchState::eNO_DATA:
			goto loop_end;

		case CaptureManager::BufferFetchState::eDEVICE_ERROR:
			soundAnalyzer.resetValues();
			goto loop_end;

		case CaptureManager::BufferFetchState::eINVALID_STATE:
			soundAnalyzer.resetValues();
			log.error(L"Unrecoverable error");
			setMeasureState(utils::MeasureState::eBROKEN);
			goto loop_end;

		default:
			log.error(L"Unexpected BufferFetchState {}", fetchResult.getState());
			setMeasureState(utils::MeasureState::eBROKEN);
			goto loop_end;
		}
	}
loop_end:

	soundAnalyzer.finish();

	return std::make_tuple(deviceManager.getDeviceStatus(), nullptr);
}

void AudioParent::_command(const wchar_t* bangArgs) {
	const isview command = bangArgs;
	if (command == L"updateDevList") {
		deviceManager.updateDeviceList();
		return;
	}

	log.error(L"unknown command '{}'", command);
}

const wchar_t* AudioParent::_resolve(int argc, const wchar_t* argv[]) {
	if (argc < 1) {
		log.error(L"Invalid section variable resolve: args needed", argc);
		return nullptr;
	}

	const isview optionName = argv[0];

	if (optionName == L"prop") {
		if (argc < 4) {
			log.error(L"Invalid section variable resolve: need >= 4 argc, but only {} found", argc);
			return nullptr;
		}

		auto channelOpt = Channel::channelParser.find(argv[1]);
		if (!channelOpt.has_value()) {
			log.error(L"Invalid section variable resolve: channel '{}' not recognized", argv[1]);
			return nullptr;
		}
		auto propVariant = soundAnalyzer.getProp(channelOpt.value(), argv[2], argv[3]);

		if (propVariant.index() == 1) {
			const auto error = std::get<1>(propVariant);
			switch (error) {
			case SoundAnalyzer::SearchError::eCHANNEL_NOT_FOUND:
				log.printer.print(L"Invalid section variable resolve: channel '{}' not found", argv[1]);
				return log.printer.getBufferPtr();

			case SoundAnalyzer::SearchError::eHANDLER_NOT_FOUND:
				log.error(L"Invalid section variable resolve: handler '{}:{}' not found", argv[1], argv[2]);
				return nullptr;

			default:
				log.error(L"Unexpected SearchError {}", error);
				return nullptr;
			}
		}
		if (propVariant.index() == 0) {
			const auto result = std::get<0>(propVariant);
			if (result == nullptr) {
				log.error(L"Invalid section variable resolve: prop '{}:{}' not found", argv[2], argv[3]);
			}

			return result;
		}

		log.error(L"Unexpected propVariant index {}", propVariant.index());
		return nullptr;
	}

	if (optionName == L"current device") {
		if (argc < 2) {
			log.error(L"Invalid section variable resolve: need >= 2 argc, but only {} found", argc);
			return nullptr;
		}

		const isview deviceProperty = argv[1];

		if (deviceProperty == L"status") {
			return deviceManager.getDeviceStatus() ? L"1" : L"0";
		}
		if (deviceProperty == L"status string") {
			return deviceManager.getDeviceStatus() ? L"active" : L"down";
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
	if (optionName == L"device list new") {
		return deviceManager.getDeviceEnumerator().getDeviceList2().c_str();
	}

	log.error(L"Invalid section variable resolve: '{}' not supported", argv[0]);
	return nullptr;
}

double AudioParent::getValue(sview id, Channel channel, index index) const {
	return soundAnalyzer.getValue(channel, id % ciView(), index);
}
