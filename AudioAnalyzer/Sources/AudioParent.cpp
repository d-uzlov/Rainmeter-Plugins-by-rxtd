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

using namespace audio_analyzer;

AudioParent::AudioParent(utils::Rainmeter&& _rain) :
	ParentBase(std::move(_rain)) {
	setUseResultString(false);

	legacyNumber = rain.read(L"MagicNumber").asInt(0);
	switch (legacyNumber) {
	case 0:
	case 104:
		break;
	default:
		logger.error(L"Fatal error: unknown MagicNumber {}", legacyNumber);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	const auto threadingParams = rain.read(L"threading").asMap(L'|', L' ');

	const bool success = helper.init(logger, threadingParams, legacyNumber);
	if (!success) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	paramParser.setRainmeter(rain);
}

void AudioParent::vReload() {
	auto request = readRequest();
	const bool anythingChanged = paramParser.parse(legacyNumber);

	if (request != requestedSource || anythingChanged) {
		requestedSource = std::move(request);

		helper.setParams(requestedSource, paramParser.getParseResult(), snapshot);

		if (!snapshot.deviceIsAvailable) {
			switch (requestedSource.sourceType) {
				using DS = CaptureManager::DataSource;
			case DS::eDEFAULT_INPUT:
				logger.error(L"No input device found");
				break;
			case DS::eDEFAULT_OUTPUT:
				logger.error(L"No output device found");
				break;
			case DS::eID:
				logger.error(
					L"Device '{} ({})' is not found",
					snapshot.diSnapshot.name,
					snapshot.diSnapshot.description
				);
				break;
			}
		}
	}
}

double AudioParent::vUpdate() {
	const bool deviceWasAvailable = snapshot.deviceIsAvailable;
	helper.update(snapshot);

	if (snapshot.fatalError) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return { };
	}

	if (!snapshot.deviceIsAvailable) {
		if (deviceWasAvailable) {
			switch (requestedSource.sourceType) {
				using DS = CaptureManager::DataSource;
			case DS::eDEFAULT_INPUT:
				logger.error(L"All input devices were disconnected or disabled");
				break;
			case DS::eDEFAULT_OUTPUT:
				logger.error(L"All output devices were disconnected or disabled");
				break;
			case DS::eID:
				logger.error(
					L"Device '{} ({})' was disconnected or disabled",
					snapshot.diSnapshot.name,
					snapshot.diSnapshot.description
				);
				break;
			}
		}

		// todo reset pictures
		return { };
	}

	for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
		auto& processingSnapshot = snapshot.dataSnapshot[procName];
		for (const auto& [handlerName, finisher] : procInfo.finishers) {
			for (auto& [channel, channelSnapshot] : processingSnapshot) {
				auto handlerDataIter = channelSnapshot.find(handlerName);
				if (handlerDataIter != channelSnapshot.end()) {
					finisher(handlerDataIter->second.handlerSpecificData);
				}
			}
		}
	}

	return snapshot.diSnapshot.status ? 1.0 : 0.0;
}

void AudioParent::vCommand(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logger.error(L"Invalid section variable: args needed");
		return;
	}

	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"current device") {
		if (args.size() < 2) {
			cl.error(L"need >= 2 argc, but only {} found", args.size());
			return;
		}

		const isview deviceProperty = args[1];
		const auto& state = snapshot.diSnapshot;

		if (deviceProperty == L"status") {
			resolveBufferString = state.status ? L"1" : L"0";
		} else if (deviceProperty == L"status string") {
			resolveBufferString = state.status ? L"active" : L"down";
		} else if (deviceProperty == L"type") {
			resolveBufferString = state.type == utils::MediaDeviceType::eINPUT ? L"input" : L"output";
		} else if (deviceProperty == L"name") {
			resolveBufferString = state.name;
		} else if (deviceProperty == L"description") {
			resolveBufferString = state.description;
		} else if (deviceProperty == L"id") {
			resolveBufferString = state.id;
		} else if (deviceProperty == L"format") {
			resolveBufferString = state.formatString;
		} else {
			cl.warning(L"unknown device property '{}'", deviceProperty);
		}

		return;
	}

	if (optionName == L"device list input") {
		resolveBufferString = snapshot.deviceListInput;
		return;
	}
	if (optionName == L"device list output") {
		resolveBufferString = snapshot.deviceListOutput;
		return;
	}
	if (optionName == L"device list") {
		resolveBufferString =
			snapshot.diSnapshot.type == utils::MediaDeviceType::eINPUT
				? snapshot.deviceListInput
				: snapshot.deviceListOutput;
		return;
	}

	if (optionName == L"value") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto ind = utils::Option{ args[4] }.asInt(0);

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		// auto procIter = saMap.find(procName);
		// if (procIter == saMap.end()) {
		// 	cl.error(L"processing '{}' is not found", procName);
		// 	return;
		// }
		//
		// const auto value = procIter->second.getAudioChildHelper().getValue(channelOpt.value(), handlerName, ind);

		const auto value = getValue(procName, handlerName, channelOpt.value(), ind);
		cl.printer.print(value);

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	if (optionName == L"handler info" || optionName == L"prop") {
		resolveProp(args, resolveBufferString);
		return;
	}

	cl.error(L"unknown section variable");
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) const {
	if (!snapshot.deviceIsAvailable) {
		return 0.0;
	}

	auto procIter = snapshot.dataSnapshot.find(proc);
	if (procIter == snapshot.dataSnapshot.end()) {
		return 0.0;
	}

	const auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channel);
	if (channelSnapshotIter == processingSnapshot.end()) {
		return 0.0;
	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(id);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		return 0.0;

	}

	auto& values = handlerSnapshotIter->second.values;
	const auto layersCount = values.getBuffersCount();
	if (layersCount == 0) {
		return 0.0;
	}
	const auto valuesCount = values.getBufferSize();
	if (ind >= valuesCount) {
		return 0.0;
	}

	return values[0][ind];
}

string AudioParent::checkHandler(isview procName, Channel channel, isview handlerName) const {
	utils::BufferPrinter bp;

	const auto procDataIter = paramParser.getParseResult().find(procName);
	if (procDataIter == paramParser.getParseResult().end()) {
		bp.print(L"processing {} is not found", procName);
		return bp.getBufferPtr();
	}

	auto procData = procDataIter->second;
	auto& channels = procData.channels;
	if (channels.find(channel) == channels.end()) {
		bp.print(L"processing {} doesn't have channel {}", procName, channel.technicalName());
		return bp.getBufferPtr();
	}

	auto& handlerMap = procData.handlersInfo.patchers;
	if (handlerMap.find(handlerName) == handlerMap.end()) {
		bp.print(L"processing {} doesn't have handler {}", procName, handlerName);
		return bp.getBufferPtr();
	}

	return { };
}

isview AudioParent::legacy_findProcessingFor(isview handlerName) const {
	for (auto& [name, pd] : paramParser.getParseResult()) {
		auto& patchers = pd.handlersInfo.patchers;
		if (patchers.find(handlerName) != patchers.end()) {
			return name;
		}
	}

	return { };
}

ParentHelper::RequestedDeviceDescription AudioParent::readRequest() const {
	ParentHelper::RequestedDeviceDescription result;

	using DataSource = CaptureManager::DataSource;
	if (const auto source = rain.read(L"Source").asIString();
		!source.empty()) {
		if (source == L"Output") {
			result.sourceType = DataSource::eDEFAULT_OUTPUT;
		} else if (source == L"Input") {
			result.sourceType = DataSource::eDEFAULT_INPUT;
		} else {
			logger.debug(L"Using '{}' as source audio device ID.", source);
			result.sourceType = DataSource::eID;
			result.id = source % csView();
		}
	} else {
		// legacy
		if (auto legacyID = this->rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			result.sourceType = DataSource::eID;
			result.id = legacyID;
		} else {
			const auto port = this->rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				result.sourceType = DataSource::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				result.sourceType = DataSource::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				result.sourceType = DataSource::eDEFAULT_OUTPUT;
			}
		}
	}

	return result;
}

void AudioParent::resolveProp(array_view<isview> args, string& resolveBufferString) {
	const isview optionName = args[0];

	isview procName = args[1];
	isview channelName = args[2];
	isview handlerName = args[3];
	isview propName = args[4];

	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"handler info") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		procName = args[1];
		channelName = args[2];
		handlerName = args[3];
		propName = args[4];
	} else if (optionName == L"prop") {
		// legacy
		if (args.size() < 4) {
			cl.error(L"need >= 4 argc, but only {} found", args.size());
			return;
		}

		channelName = args[1];
		handlerName = args[2];
		propName = args[3];

		procName = legacy_findProcessingFor(handlerName);
		if (procName.empty()) {
			cl.error(L"handler '{}' is not found", handlerName);
			return;
		}
	}

	auto channelOpt = Channel::channelParser.find(channelName);
	if (!channelOpt.has_value()) {
		cl.error(L"channel '{}' is not recognized", channelName);
		return;
	}

	auto procIter = snapshot.dataSnapshot.find(procName);
	if (procIter == snapshot.dataSnapshot.end()) {
		cl.error(L"processing '{}' is not found", procName);
		return;
	}

	auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channelOpt.value());
	if (channelSnapshotIter == processingSnapshot.end()) {
		// "channel not found" is a relaxed error, so no log message for this one
		return;
	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(handlerName);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		cl.error(L"handler '{}:{}' is not found", procName, handlerName);
		return;
	}

	auto& handlerSnapshot = handlerSnapshotIter->second;
	const SoundHandler::PropGetter propGetter
		= paramParser
		  .getParseResult()
		  .find(procName)->second.handlersInfo.patchers
		  .find(handlerName)->second->propGetter;

	if (propGetter == nullptr) {
		cl.error(L"handler '{}:{}' doesn't have any props", procName, handlerName);
		return;
	}

	const bool found = propGetter(handlerSnapshot.handlerSpecificData, propName, cl.printer);
	if (!found) {
		cl.error(L"prop '{}:{}:{}' is not found", procName, handlerName, propName);
		return;
	}

	resolveBufferString = cl.printer.getBufferView();
}
