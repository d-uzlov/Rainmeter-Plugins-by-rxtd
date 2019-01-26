/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "windows-wrappers/BufferWrapper.h"

#include <algorithm>
#include "ParamParser.h"

#pragma warning(disable : 4458)
#pragma warning(disable : 4244)

#undef max
#undef min

rxu::ParentManager<rxaa::AudioParent> rxaa::AudioParent::parentManager { };

rxaa::AudioParent::AudioParent(rxu::Rainmeter&& rain) : TypeHolder(std::move(rain)), deviceManager(log, [this](auto format) { soundAnalyzer.setWaveFormat(format); }) {
	parentManager.add(*this);

	if (!deviceManager.isObjectValid()) {
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}

	// parse port specifier
	const wchar_t *port = this->rain.readString(L"Port");
	DeviceManager::Port portEnum;
	if (_wcsicmp(port, L"") == 0 || _wcsicmp(port, L"Output") == 0) {
		portEnum = DeviceManager::Port::OUTPUT;
	} else if (_wcsicmp(port, L"Input") == 0) {
		portEnum = DeviceManager::Port::INPUT;
	} else {
		log.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
		portEnum = DeviceManager::Port::OUTPUT;
	}

	std::wstring id = this->rain.readString(L"DeviceID");

	deviceManager.setOptions(portEnum, id);
	deviceManager.init();
}

rxaa::AudioParent::~AudioParent() {
	parentManager.remove(*this);
}

rxaa::AudioParent* rxaa::AudioParent::findInstance(rxu::Rainmeter::Skin skin, const wchar_t* measureName) {
	return parentManager.findParent(skin, measureName);
}

void rxaa::AudioParent::_reload() {
	ParamParser paramParser(rain);
	paramParser.parse();

	// TODO add min target
	const auto rateLimit = rain.readInt(L"TargetRate");
	soundAnalyzer.setTargetRate(std::max<int>(0, rateLimit));

	soundAnalyzer.setPatchHandlers(paramParser.getHandlers(), paramParser.getPatches());
}

std::tuple<double, const wchar_t*> rxaa::AudioParent::_update() {
	// TODO make an option for this value?
	constexpr int maxBuffers = 15;

	for (int i = 0; i < maxBuffers; ++i) {
		const auto fetchResult = deviceManager.nextBuffer();
		switch (fetchResult.getState()) {
		case DeviceManager::BufferFetchState::OK:
		{
			auto &bufferWrapper = fetchResult.getBuffer();

			const uint8_t *const buffer = bufferWrapper.getBuffer();
			const uint32_t framesCount = bufferWrapper.getFramesCount();

			soundAnalyzer.process(buffer, bufferWrapper.isSilent(), framesCount);
			break;
		}

		case DeviceManager::BufferFetchState::NO_DATA:
			goto loop_end;

		case DeviceManager::BufferFetchState::DEVICE_ERROR:
			soundAnalyzer.resetValues();
			goto loop_end;

		case DeviceManager::BufferFetchState::INVALID_STATE:
			soundAnalyzer.resetValues();
			log.error(L"Unrecoverable error");
			setMeasureState(rxu::MeasureState::BROKEN);
			goto loop_end;

		default:
			log.error(L"Unexpected BufferFetchState {}", fetchResult.getState());
			setMeasureState(rxu::MeasureState::BROKEN);
			goto loop_end;
		}
	}
loop_end:

	return std::make_tuple(deviceManager.getDeviceStatus(), nullptr);
}

void rxaa::AudioParent::_command(const wchar_t* args) {
	const std::wstring_view command = args;
	if (command == L"updateDevList") {
		deviceManager.updateDeviceList();
		return;
	}

	log.error(L"unknown command '{}'", command);
}

const wchar_t* rxaa::AudioParent::_resolve(int argc, const wchar_t* argv[]) {
	if (argc < 1) {
		log.error(L"Invalid section variable resolve: args needed", argc);
		return nullptr;
	}

	auto& optionName = argv[0];

	if (optionName == L"prop"sv) {
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
			case SoundAnalyzer::SearchError::CHANNEL_NOT_FOUND:
				log.printer.print(L"Invalid section variable resolve: channel '{}' not found", argv[1]);
				return log.printer.getBufferPtr();

			case SoundAnalyzer::SearchError::HANDLER_NOT_FOUND:
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

	if (optionName == L"current device"sv) {
		if (argc < 2) {
			log.error(L"Invalid section variable resolve: need >= 2 argc, but only {} found", argc);
			return nullptr;
		}

		auto& deviceProperty = argv[1];

		if (deviceProperty == L"status"sv) {
			return deviceManager.getDeviceStatus() ? L"1" : L"0";
		}
		if (deviceProperty == L"status string"sv) {
			return deviceManager.getDeviceStatus() ? L"active" : L"down";
		}
		if (deviceProperty == L"name"sv) {
			return deviceManager.getDeviceName().c_str();
		}
		if (deviceProperty == L"id"sv) {
			return deviceManager.getDeviceId().c_str();
		}
		if (deviceProperty == L"format"sv) {
			return deviceManager.getDeviceFormat().c_str();
		}

		return nullptr;
	}


	if (optionName == L"device list"sv) {
		return deviceManager.getDeviceList().c_str();
	}

	log.error(L"Invalid section variable resolve: '{}' not supported", argv[0]);
	return nullptr;
}

double rxaa::AudioParent::getValue(const std::wstring& id, Channel channel, int index) const {
	return soundAnalyzer.getValue(channel, id, index);
}
