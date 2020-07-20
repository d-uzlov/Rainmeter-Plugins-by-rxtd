/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "ParamParser.h"

#include "undef.h"

using namespace audio_analyzer;

AudioParent::AudioParent(utils::Rainmeter&& rain) :
	ParentBase(std::move(rain)),
	deviceManager(logger, [this](auto format) { soundAnalyzer.setWaveFormat(format); }) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
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

double AudioParent::_update() {
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
		deviceManager.getCaptureManager().capture([&](bool silent, array_view<std::byte> buffer) {
			soundAnalyzer.process(buffer, silent);
		}, maxLoop);

		soundAnalyzer.finishStandalone();
	}

	return deviceManager.getDeviceStatus();
}

void AudioParent::_command(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStrings();
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::_resolve(int argc, const wchar_t* argv[], string& resolveBufferString) {
	if (argc < 1) {
		logger.error(L"Invalid section variable: args needed", argc);
		return;
	}

	const isview optionName = argv[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"prop") {
		if (argc < 4) {
			cl.error(L"need >= 4 argc, but only {} found", argc);
			return;
		}

		auto channelOpt = Channel::channelParser.find(argv[1]);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' not recognized", argv[1]);
			return;
		}
		auto propVariant = soundAnalyzer.getAudioChildHelper().getProp(channelOpt.value(), argv[2], argv[3]);

		if (propVariant.index() == 1) {
			const auto error = std::get<1>(propVariant);
			switch (error) {
			case AudioChildHelper::SearchError::eCHANNEL_NOT_FOUND:
				cl.printer.print(L"channel '{}' not found", argv[1]);
				resolveBufferString = cl.printer.getBufferPtr();
				return;

			case AudioChildHelper::SearchError::eHANDLER_NOT_FOUND:
				cl.error(L"handler '{}:{}' not found", argv[1], argv[2]);
				return;

			default:
				cl.error(L"unexpected SearchError value '{}'", error);
				return;
			}
		}
		if (propVariant.index() == 0) {
			const auto result = std::get<0>(propVariant);
			if (result == nullptr) {
				cl.error(L"prop '{}:{}' not found", argv[2], argv[3]);
				return;
			}

			resolveBufferString = result;
			return;
		}

		cl.error(L"unexpected propVariant index '{}'", propVariant.index());
		return;
	}

	if (optionName == L"current device") {
		if (argc < 2) {
			cl.error(L"need >= 2 argc, but only {} found", argc);
			return;
		}

		const isview deviceProperty = argv[1];

		if (deviceProperty == L"status") {
			resolveBufferString = deviceManager.getDeviceStatus() ? L"1" : L"0";
			return;
		}
		if (deviceProperty == L"status string") {
			resolveBufferString = deviceManager.getDeviceStatus() ? L"active" : L"down";
			return;
		}
		if (deviceProperty == L"type") {
			switch (deviceManager.getCurrentDeviceType()) {
			case utils::MediaDeviceType::eINPUT:
				resolveBufferString = L"input";
				return;
			case utils::MediaDeviceType::eOUTPUT:
				resolveBufferString = L"output";
				return;
			default: ;
			}
		}
		if (deviceProperty == L"name") {
			resolveBufferString = deviceManager.getDeviceInfo().fullFriendlyName;
			return;
		}
		if (deviceProperty == L"nameOnly") {
			resolveBufferString = deviceManager.getDeviceInfo().name;
			return;
		}
		if (deviceProperty == L"description") {
			resolveBufferString = deviceManager.getDeviceInfo().desc;
			return;
		}
		if (deviceProperty == L"id") {
			resolveBufferString = deviceManager.getDeviceInfo().id;
			return;
		}
		if (deviceProperty == L"format") {
			resolveBufferString = deviceManager.getCaptureManager().getFormatString();
			return;
		}

		return;
	}

	if (optionName == L"device list input") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListInput();
		return;
	}
	if (optionName == L"device list output") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListOutput();
		return;
	}

	cl.error(L"option '{}' not supported", optionName);
	return;
}

double AudioParent::getValue(sview id, Channel channel, index index) const {
	return soundAnalyzer.getAudioChildHelper().getValue(channel, id % ciView(), index);
}
