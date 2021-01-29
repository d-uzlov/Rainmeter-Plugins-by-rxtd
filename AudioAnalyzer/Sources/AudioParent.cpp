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

AudioParent::AudioParent(Rainmeter&& _rain) :
	ParentBase(std::move(_rain)) {
	setUseResultString(false);
	logHelpers.setLogger(logger);

	logHelpers.sourceTypeIsNotRecognized.setLogFunction(
		[](Logger& logger, istring type) {
			logger.error(L"Source type '{}' is not recognized", type);
		}
	);
	logHelpers.unknownCommand.setLogFunction(
		[](Logger& logger, istring command) {
			logger.error(L"unknown command '{}'", command);
		}
	);
	logHelpers.currentDeviceUnknownProp.setLogFunction(
		[](Logger& logger, istring deviceProperty) {
			logger.error(L"unknown device property '{}'", deviceProperty);
		}
	);
	logHelpers.unknownSectionVariable.setLogFunction(
		[](Logger& logger, istring optionName) {
			logger.error(L"unknown section variable '{}'", optionName);
		}
	);
	logHelpers.legacy_invalidPort.setLogFunction(
		[](Logger& logger, istring port) {
			logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
		}
	);

	logHelpers.processingNotFound.setLogFunction(
		[](Logger& logger, istring procName) {
			logger.error(L"Processing doesn't exist: {}", procName);
		}
	);
	logHelpers.channelNotRecognized.setLogFunction(
		[](Logger& logger, istring channelName) {
			logger.error(L"Channel is not recognized: {}", channelName);
		}
	);
	logHelpers.noProcessingHaveHandler.setLogFunction(
		[](Logger& logger, istring channelName) {
			logger.error(L"No processing have handler '{}'", channelName);
		}
	);
	logHelpers.processingDoesNotHaveHandler.setLogFunction(
		[](Logger& logger, istring procName, istring handlerName) {
			logger.error(L"Processing '{}' doesn't have handler '{}'", procName, handlerName);
		}
	);
	logHelpers.processingDoesNotHaveChannel.setLogFunction(
		[](Logger& logger, istring procName, istring channelName) {
			logger.error(L"Processing '{}' doesn't have channel '{}'", procName, channelName);
		}
	);
	logHelpers.handlerDoesNotHaveProps.setLogFunction(
		[](Logger& logger, istring type) {
			logger.error(L"handler type '{}' doesn't have any props", type);
		}
	);
	logHelpers.propNotFound.setLogFunction(
		[](Logger& logger, istring type, istring propName) {
			logger.error(L"Handler type '{}' doesn't have info '{}'", type, propName);
		}
	);

	legacyNumber = rain.read(L"MagicNumber").asInt(0);
	switch (legacyNumber) {
	case 0:
	case 104:
		break;
	default:
		logger.error(L"Fatal error: unknown MagicNumber {}", legacyNumber);
		throw std::runtime_error{""};
	}

	if (legacyNumber < 104) {
		logger.warning(L"Measure operated in legacy mode. If you are a skin author, consider modernizing it.");
	}

	try {
		const auto threadingParams = rain.read(L"threading").asMap(L'|', L' ');
		helper.init(rain, logger, threadingParams, legacyNumber);
		const auto untouchedOptions = threadingParams.getListOfUntouched();
		if (!untouchedOptions.empty()) {
			logger.warning(L"Threading: unused options: {}", untouchedOptions);
		}
	} catch (std::exception&) {
		throw std::runtime_error{ "" };
	}

	paramParser.setRainmeter(rain);
}

void AudioParent::vReload() {
	const auto oldSource = requestedSource;
	requestedSource = readRequest();

	if (oldSource != requestedSource && !requestedSource.has_value()) {
		helper.setInvalid();
		return;
	}

	const bool paramsChanged = paramParser.parse(legacyNumber, false);

	if (paramsChanged) {
		updateCleaners();
	}

	const auto oldCallbacks = std::move(callbacks);
	callbacks.onUpdate = rain.read(L"callback-onUpdate", false).asString();
	callbacks.onDeviceChange = rain.read(L"callback-onDeviceChange", false).asString();
	callbacks.onDeviceListChange = rain.read(L"callback-onDeviceListChange", false).asString();
	callbacks.onDeviceDisconnected = rain.read(L"callback-onDeviceDisconnected", false).asString();

	if (oldCallbacks != callbacks || oldSource != requestedSource || paramsChanged) {
		std::optional<ParamParser::ProcessingsInfoMap> paramsOpt = {};
		if (paramsChanged) {
			paramsOpt = paramParser.getParseResult();
		}

		helper.setParams(callbacks, requestedSource, std::move(paramsOpt));

		logHelpers.reset();
	}
}

double AudioParent::vUpdate() {
	if (requestedSource.has_value()) {
		helper.update();
	}

	if (!helper.getSnapshot().deviceIsAvailable.load()) {
		if (!cleanersExecuted) {
			runCleaners();
			cleanersExecuted = true;
		}
		return {};
	} else {
		cleanersExecuted = false;
	}

	{
		auto& data = helper.getSnapshot().data;
		auto lock = data.getLock();

		for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
			auto iter = data._.find(procName);
			if (iter == data._.end()) {
				continue;
			}

			auto& processingSnapshot = iter->second;
			for (const auto& [handlerName, finisher] : procInfo.finishers) {
				for (auto& [channel, channelSnapshot] : processingSnapshot) {
					auto handlerDataIter = channelSnapshot.find(handlerName);
					if (handlerDataIter != channelSnapshot.end()) {
						runFinisher(
							finisher, handlerDataIter->second.handlerSpecificData, procName, channel,
							handlerName
						);
					}
				}
			}
		}
	}

	return 1.0;
}

void AudioParent::vCommand(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		return;
	}

	logHelpers.unknownCommand.log(bangArgs);
}

void AudioParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logHelpers.generic.log(L"Invalid section variable: at least 1 argument is needed");
		return;
	}

	const isview optionName = args[0];

	if (optionName == L"current device") {
		if (!requestedSource.has_value()) {
			return;
		}

		if (args.size() < 2) {
			logHelpers.generic.log(L"'current device' section variable need 2 args, but only 1 is found");
			return;
		}

		auto& info = helper.getSnapshot().deviceInfo;
		auto lock = info.getLock();
		const auto& di = info._;

		const isview deviceProperty = args[1];

		if (deviceProperty == L"detailedState") {
			switch (di.state) {
			case CaptureManager::State::eOK:
				resolveBufferString = L"1";
				break;
			case CaptureManager::State::eDEVICE_CONNECTION_ERROR:
			case CaptureManager::State::eRECONNECT_NEEDED:
				resolveBufferString = L"2";
				break;
			case CaptureManager::State::eMANUALLY_DISCONNECTED:
				resolveBufferString = L"3";
				break;
			case CaptureManager::State::eDEVICE_IS_EXCLUSIVE:
				resolveBufferString = L"4";
				break;
			}
			return;
		}

		if (deviceProperty == L"detailedStateString") {
			switch (di.state) {
			case CaptureManager::State::eOK:
				resolveBufferString = L"ok";
				break;
			case CaptureManager::State::eDEVICE_CONNECTION_ERROR:
			case CaptureManager::State::eRECONNECT_NEEDED:
				resolveBufferString = L"connectionError";
				break;
			case CaptureManager::State::eMANUALLY_DISCONNECTED:
				resolveBufferString = L"disconnected";
				break;
			case CaptureManager::State::eDEVICE_IS_EXCLUSIVE:
				resolveBufferString = L"exclusive";
				break;
			}
			return;
		}

		if (deviceProperty == L"status") {
			resolveBufferString = di.state == CaptureManager::State::eOK ? L"1" : L"0";
			return;
		}
		if (deviceProperty == L"status string") {
			resolveBufferString = di.state == CaptureManager::State::eOK ? L"active" : L"down";
			return;
		}

		if (di.state != CaptureManager::State::eOK) {
			return;
		}

		if (deviceProperty == L"type") {
			resolveBufferString = di.type == utils::MediaDeviceType::eINPUT ? L"input" : L"output";
		} else if (deviceProperty == L"name") {
			resolveBufferString = di.name;
		} else if (deviceProperty == L"description") {
			resolveBufferString = di.description;
		} else if (deviceProperty == L"id") {
			resolveBufferString = di.id;
		} else if (deviceProperty == L"format") {
			resolveBufferString = di.formatString;
		} else if (deviceProperty == L"channels") {
			resolveBufferString = di.channelsString;
		} else if (deviceProperty == L"sample rate") {
			resolveBufferString = std::to_wstring(di.format.samplesPerSec);
		} else {
			logHelpers.currentDeviceUnknownProp.log(deviceProperty);
		}

		return;
	}

	if (optionName == L"device list input") {
		auto& lists = helper.getSnapshot().deviceLists;
		auto lock = lists.getLock();
		resolveBufferString = lists.input;
		return;
	}
	if (optionName == L"device list output") {
		auto& lists = helper.getSnapshot().deviceLists;
		auto lock = lists.getLock();
		resolveBufferString = lists.output;
		return;
	}
	if (optionName == L"device list") {
		auto& snapshot = helper.getSnapshot();
		auto listsLock = snapshot.deviceLists.getLock();
		auto diLock = snapshot.deviceInfo.getLock();
		resolveBufferString =
			snapshot.deviceInfo._.type == utils::MediaDeviceType::eINPUT
			? snapshot.deviceLists.input
			: snapshot.deviceLists.output;
		return;
	}

	if (optionName == L"value" || optionName == L"handlerInfo") {
		if (optionName == L"value") {
			resolveBufferString = L"0";

			if (!helper.getSnapshot().deviceIsAvailable) {
				return;
			}
		}
		if (!requestedSource.has_value()) {
			// if requestedSource.has_value() then
			//	even if handlers are not valid due to device connection error
			// we should return correct prop values
			// because someone could try to read file names
			// before we actually connect to device and make handlers valid
			return;
		}

		auto map = utils::Option{ args[1] }.asMap(L'|', L' ');

		auto procName = map.get(L"proc").asIString();
		const auto channelName = map.get(L"channel").asIString();
		const auto handlerName = map.get(L"handler").asIString();

		if (channelName.empty()) {
			if (optionName == L"value") {
				logHelpers.generic.log(L"Resolve 'value' requires channel to be specified");
			} else {
				logHelpers.generic.log(L"Resolve 'handlerInfo' requires channel to be specified");
			}
			return;
		}

		if (handlerName.empty()) {
			if (optionName == L"value") {
				logHelpers.generic.log(L"Resolve 'value' requires handler to be specified");
			} else {
				logHelpers.generic.log(L"Resolve 'handlerInfo' requires handler to be specified");
			}
			return;
		}

		auto channelOpt = ChannelUtils::parse(channelName);
		if (!channelOpt.has_value()) {
			logHelpers.channelNotRecognized.log(channelName);
			return;
		}

		if (procName.empty()) {
			procName = findProcessingFor(handlerName);

			if (procName.empty()) {
				return;
			}
		}

		if (optionName == L"value") {
			if (!isHandlerShouldExist(procName, channelOpt.value(), handlerName)) {
				// handlerInfo has its own check for isHandlerShouldExist
				// inside #resolveProp()
				return;
			}

			const auto ind = map.get(L"index").asInt(0);

			common::buffer_printer::BufferPrinter bp;
			const auto value = getValue(procName, handlerName, channelOpt.value(), ind);
			bp.print(value);
			resolveBufferString = bp.getBufferView();
			return;
		}
		if (optionName == L"handlerInfo") {
			auto propName = map.get(L"data").asIString();
			if (propName.empty()) {
				logHelpers.generic.log(L"handlerInfo must have 'prop' specified");
				return;
			}
			resolveProp(resolveBufferString, procName, channelOpt.value(), handlerName, propName);
			return;
		}
		return;
	}

	if (optionName == L"prop") {
		if (!requestedSource.has_value()) {
			// if requestedSource.has_value() then
			//	even if handlers are not valid due to device connection error
			// we should return correct prop values
			// because someone could try to read file names
			// before we actually connect to device and make handlers valid
			return;
		}

		// legacy
		if (args.size() != 4) {
			logHelpers.generic.log(L"'prop' need 4 arguments");
			return;
		}

		const auto channelName = args[1];
		const auto handlerName = args[2];
		const auto propName = args[3];

		auto channelOpt = ChannelUtils::parse(channelName);
		if (!channelOpt.has_value()) {
			logHelpers.channelNotRecognized.log(channelName);
			return;
		}

		auto procName = findProcessingFor(handlerName);
		if (procName.empty()) {
			return;
		}

		resolveProp(resolveBufferString, procName, channelOpt.value(), handlerName, propName);
		return;
	}

	logHelpers.unknownSectionVariable.log(optionName);
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) {
	if (!helper.getSnapshot().deviceIsAvailable) {
		return 0.0;
	}

	auto& data = helper.getSnapshot().data;
	auto lock = data.getLock();

	auto procIter = data._.find(proc);
	if (procIter == data._.end()) {
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

bool AudioParent::isHandlerShouldExist(isview procName, Channel channel, isview handlerName) const {
	const auto procDataIter = paramParser.getParseResult().find(procName);
	if (procDataIter == paramParser.getParseResult().end()) {
		logHelpers.processingNotFound.log(procName);
		return false;
	}

	auto procData = procDataIter->second;
	auto& channels = procData.channels;
	if (channels.find(channel) == channels.end()) {
		logHelpers.processingDoesNotHaveChannel.log(procName, ChannelUtils::getTechnicalName(channel) % ciView());
		return false;
	}

	auto& handlerMap = procData.handlersInfo.patchers;
	if (handlerMap.find(handlerName) == handlerMap.end()) {
		logHelpers.processingDoesNotHaveHandler.log(procName, handlerName);
		return false;
	}

	return true;
}

isview AudioParent::findProcessingFor(isview handlerName) const {
	for (auto& [name, pd] : paramParser.getParseResult()) {
		auto& patchers = pd.handlersInfo.patchers;
		if (patchers.find(handlerName) != patchers.end()) {
			return name;
		}
	}

	logHelpers.noProcessingHaveHandler.log(handlerName);

	return {};
}

void AudioParent::updateCleaners() {
	for (auto& [processingName, pd] : paramParser.getParseResult()) {
		auto newCleaners = createCleanersFor(pd);
		auto& oldCleaners = cleanersMap[processingName];
		for (auto& [handlerName, data] : oldCleaners) {
			if (newCleaners.find(handlerName) == newCleaners.end()) {
				// clean old images
				if (data.finisher != nullptr) {
					for (auto channel : pd.channels) {
						runFinisher(data.finisher, data.data, processingName, channel, handlerName);
					}
				}
			}
		}
		for (auto& [handlerName, data] : newCleaners) {
			if (oldCleaners.find(handlerName) == oldCleaners.end()) {
				// draw placeholders for new images
				if (data.finisher != nullptr) {
					for (auto channel : pd.channels) {
						auto dataCopy = data.data;
						runFinisher(data.finisher, dataCopy, processingName, channel, handlerName);
					}
				}
			}
		}
		// handlers that exist both in old and new options should be fine without any actions

		oldCleaners = std::move(newCleaners);
	}
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
				logHelpers.legacy_invalidPort.log(port);
				result.type = ST::eDEFAULT_OUTPUT;
			}
		}
	} else {
		auto sourceOpt = rain.read(L"Source");
		const auto source = sourceOpt.asIString(L"DefaultOutput");

		if (source == L"Output") {
			logHelpers.generic.log(L"Source type 'Output' is not recognized. Did you mean DefaultOutput?");
			return {};
		}
		if (source == L"Input") {
			logHelpers.generic.log(L"Source type 'Input' is not recognized. Did you mean DefaultInput?");
			return {};
		}

		if (source == L"DefaultOutput") {
			result.type = ST::eDEFAULT_OUTPUT;
		} else if (source == L"DefaultInput") {
			result.type = ST::eDEFAULT_INPUT;
		} else if (auto [type, value] = sourceOpt.breakFirst(L':');
			type.asIString() == L"id") {
			if (value.empty()) {
				logHelpers.generic.log(L"device ID can not be empty");
				return {};
			}

			result.type = ST::eID;
			result.id = value.asString();
		} else {
			logHelpers.sourceTypeIsNotRecognized.log(type.asIString());
			return {};
		}
	}

	return result;
}

void AudioParent::resolveProp(
	string& resolveBufferString,
	isview procName, Channel channel, isview handlerName, isview propName
) {
	if (!isHandlerShouldExist(procName, channel, handlerName)) {
		return;
	}

	const auto procIter0 = paramParser.getParseResult().find(procName);
	const auto handlerInfoIter = procIter0->second.handlersInfo.patchers.find(handlerName);

	const PatchInfo* const handlerInfo = &handlerInfoIter->second;
	const auto propGetter = handlerInfo->externalMethods.getProp;

	if (propGetter == nullptr) {
		logHelpers.handlerDoesNotHaveProps.log(handlerInfo->type);
		return;
	}

	auto& data = helper.getSnapshot().data;
	auto lock = data.getLock();

	// "not found" errors below are not logged because we have already checked everything above,
	// and if we still don't find requested info then it is caused either by delay in updating second thread
	// or by device not having requested channel

	SoundHandler::ExternalData* handlerExternalData = nullptr;

	if (auto procIter = data._.find(procName);
		procIter != data._.end()) {

		auto& processingSnapshot = procIter->second;

		if (auto channelSnapshotIter = processingSnapshot.find(channel);
			channelSnapshotIter != processingSnapshot.end()) {
			auto& channelSnapshot = channelSnapshotIter->second;

			if (auto handlerSnapshotIter = channelSnapshot.find(handlerName);
				handlerSnapshotIter != channelSnapshot.end()) {
				handlerExternalData = &handlerSnapshotIter->second.handlerSpecificData;
			}
		}
	}
	if (handlerExternalData == nullptr) {
		// we can access paramParser values here without checks
		// because isHandlerShouldExist above checked that it should be valid to access them
		handlerExternalData = &cleanersMap.find(procName)->second.find(handlerName)->second.data;
	}

	if (handlerExternalData == nullptr) {
		// we should never get here
		// but just it case there is an error, let's make sure that plugin won't crash the application
		logHelpers.generic.log(L"unexpected (handlerSpecificData == nullptr), tell the developer about this error");
		return;
	}


	SoundHandler::ExternCallContext context;
	context.legacyNumber = legacyNumber;

	context.channelName =
		legacyNumber < 104
		? ChannelUtils::getTechnicalNameLegacy(channel)
		: ChannelUtils::getTechnicalName(channel);

	string filePrefix;
	filePrefix += procName % csView();
	filePrefix += L'-';
	filePrefix += handlerName % csView();
	filePrefix += L'-';
	filePrefix += context.channelName;

	context.filePrefix = filePrefix;

	common::buffer_printer::BufferPrinter bp;
	const bool found = propGetter(*handlerExternalData, propName, bp, context);
	if (!found) {
		logHelpers.propNotFound.log(handlerInfo->type, propName);
		return;
	}

	resolveBufferString = bp.getBufferView();
}

AudioParent::ProcessingCleanersMap AudioParent::createCleanersFor(const ParamParser::ProcessingData& pd) const {
	std::set<Channel> channels = pd.channels;

	using HandlerMap = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;

	ProcessingManager::ChannelSnapshot tempSnapshot;
	HandlerMap tempChannelMap;

	for (auto& handlerName : pd.handlersInfo.order) {
		auto& patchInfo = pd.handlersInfo.patchers.find(handlerName)->second;

		auto handlerPtr = patchInfo.fun({});

		auto cl = logger.getSilent();
		ProcessingManager::HandlerFinderImpl hf{ tempChannelMap };
		SoundHandler::Snapshot handlerSpecificData;
		const bool success = handlerPtr->patch(
			patchInfo.params, patchInfo.sources,
			48000, legacyNumber,
			hf, cl,
			handlerSpecificData
		);

		if (success) {
			tempChannelMap[handlerName] = std::move(handlerPtr);
			tempSnapshot[handlerName] = std::move(handlerSpecificData);
		}
	}

	ProcessingCleanersMap result;
	for (const auto& [handlerName, finisher] : pd.finishers) {
		auto dataIter = tempSnapshot.find(handlerName);
		if (dataIter != tempSnapshot.end()) {
			result[handlerName] = { std::move(dataIter->second.handlerSpecificData), finisher };
		}
	}

	return result;
}

void AudioParent::runCleaners() const {
	for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
		auto processingCleanersIter = cleanersMap.find(procName);
		if (processingCleanersIter == cleanersMap.end()) {
			continue;
		}

		auto& processingCleaners = processingCleanersIter->second;
		for (const auto& [handlerName, finisher] : procInfo.finishers) {
			auto handlerDataIter = processingCleaners.find(handlerName);
			if (handlerDataIter == processingCleaners.end()) {
				continue;
			}
			for (auto channel : procInfo.channels) {
				auto dataCopy = handlerDataIter->second.data;
				runFinisher(finisher, dataCopy, procName, channel, handlerName);
			}
		}
	}
}
