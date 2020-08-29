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

	try {
		const auto threadingParams = rain.read(L"threading").asMap(L'|', L' ');
		helper.init(logger, threadingParams, legacyNumber);
		const auto untouchedOptions = threadingParams.getListOfUntouched();
		if (!untouchedOptions.empty()) {
			logger.warning(L"Threading: unused options: {}", untouchedOptions);
		}
	} catch (std::exception& e) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	paramParser.setRainmeter(rain);

	paramParser.parse(legacyNumber, true);
	for (auto& [processingName, pd] : paramParser.getParseResult()) {
		cleanersMap[processingName] = createCleanersFor(pd);
	}
	runCleaners();
	cleanersExecuted = true;
}

void AudioParent::vReload() {
	const auto oldSource = requestedSource;
	requestedSource = readRequest();

	if (oldSource != requestedSource && !requestedSource.has_value()) {
		helper.setInvalid();
		snapshot.deviceIsAvailable = false;
		return;
	}

	const bool paramsChanged = paramParser.parse(legacyNumber, false) || firstReload;

	if (oldSource != requestedSource || paramsChanged) {
		firstReload = false;

		std::optional<ParamParser::ProcessingsInfoMap> paramsOpt = { };
		if (paramsChanged || !oldSource.has_value()) {
			paramsOpt = paramParser.getParseResult();
		}

		helper.setParams(requestedSource, std::move(paramsOpt));
		snapshot.data = { };
	}
}

double AudioParent::vUpdate() {
	if (requestedSource.has_value()) {
		helper.update(snapshot);
	}

	if (!snapshot.deviceIsAvailable) {
		if (!cleanersExecuted) {
			runCleaners();
			cleanersExecuted = true;
		}
		return { };
	} else {
		cleanersExecuted = false;
	}

	for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
		auto& processingSnapshot = snapshot.data[procName];
		for (const auto& [handlerName, finisher] : procInfo.finishers) {
			for (auto& [channel, channelSnapshot] : processingSnapshot) {
				auto handlerDataIter = channelSnapshot.find(handlerName);
				if (handlerDataIter != channelSnapshot.end()) {
					finisher(handlerDataIter->second.handlerSpecificData);
				}
			}
		}
	}

	return snapshot.deviceInfo.state == CaptureManager::State::eOK ? 1.0 : 0.0;
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
		if (!requestedSource.has_value()) {
			return;
		}

		if (args.size() < 2) {
			cl.error(L"need >= 2 argc, but only {} found", args.size());
			return;
		}

		const isview deviceProperty = args[1];
		const auto& state = snapshot.deviceInfo;

		if (deviceProperty == L"status") {
			resolveBufferString = state.state == CaptureManager::State::eOK ? L"1" : L"0";
		} else if (deviceProperty == L"status string") {
			resolveBufferString = state.state == CaptureManager::State::eOK ? L"active" : L"down";
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
		resolveBufferString = snapshot.deviceLists.input;
		return;
	}
	if (optionName == L"device list output") {
		resolveBufferString = snapshot.deviceLists.output;
		return;
	}
	if (optionName == L"device list") {
		resolveBufferString =
			snapshot.deviceInfo.type == utils::MediaDeviceType::eINPUT
				? snapshot.deviceLists.input
				: snapshot.deviceLists.output;
		return;
	}

	if (optionName == L"value") {
		if (!requestedSource.has_value()) {
			return;
		}

		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto ind = utils::Option{ args[4] }.asInt(0);

		auto channelOpt = ChannelUtils::parse(channelName);
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
		if (!requestedSource.has_value()) {
			return;
		}

		resolveProp(args, resolveBufferString);
		return;
	}

	cl.error(L"unknown section variable");
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) const {
	if (!snapshot.deviceIsAvailable) {
		return 0.0;
	}

	auto procIter = snapshot.data.find(proc);
	if (procIter == snapshot.data.end()) {
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
		bp.print(L"processing {} doesn't have channel {}", procName, ChannelUtils::getTechnicalName(channel));
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

AudioParent::DeviceRequest AudioParent::readRequest() const {
	CaptureManager::SourceDesc result;

	using ST = CaptureManager::SourceDesc::Type;
	if (legacyNumber < 104) {
		if (auto legacyID = rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			result.type = ST::eID;
			result.id = std::move(legacyID);
		} else {
			const auto port = rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				result.type = ST::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				result.type = ST::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				result.type = ST::eDEFAULT_OUTPUT;
			}
		}
	} else {
		auto sourceOpt = rain.read(L"Source");
		const auto source = sourceOpt.asIString(L"DefaultOutput");
		if (source == L"DefaultOutput") {
			result.type = ST::eDEFAULT_OUTPUT;
		} else if (source == L"DefaultInput") {
			result.type = ST::eDEFAULT_INPUT;
		} else if (auto [type, value] = sourceOpt.breakFirst(L':');
			type.asIString() == L"id") {
			result.type = ST::eID;
			result.id = source % csView();
		} else {
			logger.error(L"Source type '{}' is not recognized", type.asString());
			return { };
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

	auto channelOpt = ChannelUtils::parse(channelName);
	if (!channelOpt.has_value()) {
		cl.error(L"channel '{}' is not recognized", channelName);
		return;
	}

	auto procIter = snapshot.data.find(procName);
	if (procIter == snapshot.data.end()) {
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
	const auto propGetter
		= paramParser
		  .getParseResult()
		  .find(procName)->second.handlersInfo.patchers
		  .find(handlerName)->second.externalMethods.getProp;

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

AudioParent::ProcessingCleanersMap AudioParent::createCleanersFor(const ParamParser::ProcessingData& pd) const {
	std::set<Channel> channels = pd.channels;

	using HandlerMap = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;
	struct HandlerMapStruct {
		HandlerMap handlerMap;
	};

	ProcessingManager::Snapshot tempSnapshot;
	std::map<Channel, HandlerMapStruct> tempChannelMap;

	for (auto& handlerName : pd.handlersInfo.order) {
		auto& patchInfo = pd.handlersInfo.patchers.find(handlerName)->second;

		bool handlerIsValid = true;
		for (auto channel : channels) {
			auto& channelDataNew = tempChannelMap[channel].handlerMap;
			auto handlerPtr = patchInfo.fun({ });

			auto cl = logger.silent();
			ProcessingManager::HandlerFinderImpl hf{ channelDataNew };
			SoundHandler::Snapshot handlerSpecificData;
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				ChannelUtils::getTechnicalName(channel), 48000,
				hf, cl,
				handlerSpecificData
			);

			if (success) {
				channelDataNew[handlerName] = std::move(handlerPtr);
				tempSnapshot[channel][handlerName] = std::move(handlerSpecificData);
				handlerIsValid = false;
			} else {
				break;
			}
		}
	}

	ProcessingCleanersMap result;
	for (const auto& [handlerName, finisher] : pd.finishers) {
		for (auto& [channel, channelSnapshot] : tempSnapshot) {
			auto dataIter = channelSnapshot.find(handlerName);
			if (dataIter != channelSnapshot.end()) {
				result[channel][handlerName] = std::move(dataIter->second.handlerSpecificData);
			}
		}
	}

	return result;
}

void AudioParent::runCleaners() const {
	for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
		auto processingCleanersIter = cleanersMap.find(procName);
		if (processingCleanersIter == cleanersMap.end()) {
			// in case options were updated
			// we only generate cleaners for initial options
			// so if someone changes them and add more channels,
			// then cleanersMap won't contain some of the channels from paramParser
			continue;
		}

		auto& processingCleaners = processingCleanersIter->second;
		for (const auto& [handlerName, finisher] : procInfo.finishers) {
			for (auto& [channel, channelSnapshot] : processingCleaners) {
				auto handlerDataIter = channelSnapshot.find(handlerName);
				if (handlerDataIter == channelSnapshot.end()) {
					continue;
				}

				std::any dataCopy = handlerDataIter->second;
				finisher(dataCopy);
			}
		}
	}
}
