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

AudioParent::AudioParent(utils::Rainmeter&& _rain) :
	ParentBase(std::move(_rain)),
	deviceManager(logger, [this](MyWaveFormat format) {
		channelMixer.setFormat(format);
		for (auto& analyzer : analyzers) {
			analyzer.setSourceRate(format.samplesPerSec);
			analyzer.setLayout(format.channelLayout);
		}
		currentFormat = std::move(format);
	}) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

void AudioParent::_reload() {
	const auto source = rain.read(L"Source").asIString();
	string id = { };
	DataSource sourceEnum;
	if (!source.empty()) {
		if (source == L"Output") {
			sourceEnum = DataSource::eDEFAULT_OUTPUT;
		} else if (source == L"Input") {
			sourceEnum = DataSource::eDEFAULT_INPUT;
		} else {
			logger.debug(L"Using '{}' as source audio device ID.", source);
			sourceEnum = DataSource::eID;
			id = source % csView();
		}
	} else {
		// legacy
		if (auto legacyID = this->rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			logger.debug(L"Using '{}' as source audio device ID.", legacyID);
			sourceEnum = DataSource::eID;
			id = legacyID;
		} else {
			const auto port = this->rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				sourceEnum = DataSource::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			}
		}
	}

	deviceManager.setOptions(sourceEnum, id);

	ParamParser paramParser(rain, rain.read(L"UnusedOptionsWarning").asBool(true));
	auto processings = paramParser.parse();

	analyzers.reserve(processings.size());
	// TODO save state on reload
	for (auto& procInfo : processings) {
		auto& a = analyzers.emplace_back(channelMixer, logger);
		a.getCPH().setTargetRate(procInfo.targetRate);
		a.getCPH().setFCC(std::move(procInfo.fcc));
		a.setHandlers(procInfo.channels, procInfo.handlerInfo);

		a.setSourceRate(currentFormat.samplesPerSec);
		a.setLayout(currentFormat.channelLayout);
	}

	auto targetRate = rain.read(L"TargetRate").asInt(44100);
	if (targetRate < 0) {
		logger.warning(L"Invalid TargetRate {}, must be > 0, assume 0.", targetRate);
		targetRate = 0;
	}
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

		for (auto& analyzer : analyzers) {
			analyzer.resetValues();
		}
	} else {
		deviceManager.getCaptureManager().capture([&](bool silent, array_view<std::byte> buffer) {
			if (!silent) {
				channelMixer.decomposeFramesIntoChannels(buffer, true);
			}
			for (auto& analyzer : analyzers) {
				analyzer.process(silent);
			}
		}, maxLoop);

		for (auto& analyzer : analyzers) {
			analyzer.finishStandalone();
		}
	}

	return deviceManager.getDeviceStatus();
}

void AudioParent::_command(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(deviceManager.getCurrentDeviceType());
		deviceManager.getDeviceEnumerator().updateDeviceStrings();
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::_resolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logger.error(L"Invalid section variable: args needed");
		return;
	}

	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"prop") {
		if (args.size() < 4) {
			cl.error(L"need >= 4 argc, but only {} found", args.size());
			return;
		}

		const auto channelName = args[1];
		const auto handlerName = args[2];
		const auto propName = args[3];

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' not recognized", channelName);
			return;
		}

		auto[handler, helper] = findHandlerByName(handlerName, channelOpt.value());
		if (handler == nullptr) {
			cl.error(L"handler '{}:{}' not found", channelName, handlerName);
			return;
		}

		const bool found = handler->getProp(propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	if (optionName == L"current device") {
		if (args.size() < 2) {
			cl.error(L"need >= 2 argc, but only {} found", args.size());
			return;
		}

		const isview deviceProperty = args[1];

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

	if (optionName == L"device list") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListLegacy();
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

double AudioParent::getValue(isview id, Channel channel, index ind) const {
	auto [handler, helper ] = findHandlerByName(id, channel);
	if (handler == nullptr) {
		return 0.0;
	}
	return helper->getValueFrom(handler, channel, ind);
}

std::pair<SoundHandler*, const AudioChildHelper*> AudioParent::findHandlerByName(isview name, Channel channel) const {
	for (auto& analyzer : analyzers) {
		auto handlerVar = analyzer.getAudioChildHelper().findHandler(channel, name);
		if (handlerVar.index() == 0) {
			return {
				std::get<0>(handlerVar),
				&analyzer.getAudioChildHelper(),
			};
		}
	}

	return { };
}
