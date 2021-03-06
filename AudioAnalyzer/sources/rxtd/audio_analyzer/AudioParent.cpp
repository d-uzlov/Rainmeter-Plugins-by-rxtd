﻿// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "AudioParent.h"

#include "rxtd/std_fixes/MapUtils.h"

using rxtd::audio_analyzer::AudioParent;
using rxtd::audio_analyzer::options::ParamHelper;
using rxtd::audio_analyzer::options::HandlerInfo;

void AudioParent::LogHelpers::setLogger(Logger logger) {
	generic.setLogger(logger);
	sourceTypeIsNotRecognized.setLogger(logger);
	unknownCommand.setLogger(logger);
	currentDeviceUnknownProp.setLogger(logger);
	unknownSectionVariable.setLogger(logger);
	processingNotFound.setLogger(logger);
	channelNotRecognized.setLogger(logger);
	noProcessingHaveHandler.setLogger(logger);
	processingDoesNotHaveChannel.setLogger(logger);
	handlerDoesNotHaveProps.setLogger(logger);
	propNotFound.setLogger(logger);
}

void AudioParent::LogHelpers::reset() {
	// helpers that are commented don't need to be reset
	// because it doesn't make sense to repeat their messages after measure is reloaded

	// generic.reset();
	// sourceTypeIsNotRecognized.reset();
	// unknownCommand.reset();
	// currentDeviceUnknownProp.reset();
	// unknownSectionVariable.reset();

	processingNotFound.reset();
	// channelNotRecognized.reset();
	noProcessingHaveHandler.reset();
	processingDoesNotHaveChannel.reset();
	// handlerDoesNotHaveProps.reset();
	// propNotFound.reset();
}

AudioParent::AudioParent(Rainmeter&& _rain) :
	ParentMeasureBase(std::move(_rain)) {
	setUseResultString(false);
	initLogHelpers();

	if (!rain.read(L"Changes").empty()) {
		logger.error(L"changes: no changes are allowed in this version");
		throw std::runtime_error{ "" };
	}

	parser.setLogger(logger);

	bool blockCaptureLoudnessChange;
	auto blockCaptureLoudness = rain.read(L"LockCaptureVolumeOnMax").asIString(L"never");
	if (blockCaptureLoudness == L"never") {
		blockCaptureLoudnessChange = false;
	} else if (blockCaptureLoudness == L"ForApp") {
		blockCaptureLoudnessChange = true;
	} else {
		logger.error(L"LockCaptureVolumeOnMax: value '{}' is not recognized, supported values are: Never, ForApp", blockCaptureLoudness);
		throw std::runtime_error{ "" };
	}

	paramHelper.setParser(parser);

	const auto threadingParams = rain.read(L"threading").asMap(L'|', L' ');
	auto onDeviceListChange = rain.read(L"callback-onDeviceListChange", false).asString();
	helper.init(rain, logger, threadingParams, parser, version, blockCaptureLoudnessChange, onDeviceListChange);
	const auto untouchedOptions = threadingParams.getListOfUntouched();
	if (!untouchedOptions.empty()) {
		logger.warning(L"threading: unused options: {}", untouchedOptions);
	}

	paramHelper.setRainmeter(rain);
}

void AudioParent::vReload() {
	const auto oldSource = requestedSource;
	requestedSource = readRequest();

	DeviceRequest sourceOpt;

	if (oldSource != requestedSource) {
		if (!requestedSource.has_value()) {
			helper.setInvalid();
			return;
		} else {
			sourceOpt = requestedSource;
		}
	}

	bool paramsChanged;
	try {
		paramsChanged = paramHelper.readOptions(version);
	} catch (ParamHelper::InvalidOptionsException&) {
		return;
	}

	if (paramsChanged) {
		using std_fixes::MapUtils;
		MapUtils::intersectKeyCollection(clearProcessings, paramHelper.getParseResult());
		MapUtils::intersectKeyCollection(clearSnapshot, paramHelper.getParseResult());

		for (const auto& [name, data] : paramHelper.getParseResult()) {
			auto& sa = clearProcessings[name];
			ProcessingManager::Snapshot& snapshot = clearSnapshot[name];
			sa.setParams(
				logger.getSilent(),
				data,
				version, 48000, data.channels,
				snapshot
			);
		}
	}

	const auto oldCallbacks = std::move(callbacks);
	callbacks.onUpdate = rain.read(L"callback-onUpdate", false).asString();
	callbacks.onDeviceChange = rain.read(L"callback-onDeviceChange", false).asString();
	callbacks.onDeviceListChange = rain.read(L"callback-onDeviceListChange", false).asString();
	callbacks.onDeviceDisconnected = rain.read(L"callback-onDeviceDisconnected", false).asString();

	if (oldCallbacks != callbacks || oldSource != requestedSource || paramsChanged) {
		std::optional<ParamHelper::ProcessingsInfoMap> paramsOpt = {};
		if (paramsChanged) {
			paramsOpt = paramHelper.getParseResult();
		}

		helper.setParams(callbacks, std::move(sourceOpt), std::move(paramsOpt));

		logHelpers.reset();
	}
}

double AudioParent::vUpdate() {
	helper.assertNoExceptions();

	if (requestedSource.has_value()) {
		helper.update();
	}

	if (!helper.getSnapshot().deviceIsAvailable.load()) {
		if (!cleanersExecuted) {
			runFinishers(clearSnapshot);
			cleanersExecuted = true;
		}
		return {};
	} else {
		cleanersExecuted = false;
	}

	{
		auto& snapshotData = helper.getSnapshot().data;
		auto lock = snapshotData.getLock();

		runFinishers(snapshotData._);
	}

	return 1.0;
}

void AudioParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logHelpers.generic.log(L"resolve: at least 1 argument is required");
		setInvalid(true);
		return;
	}

	const isview optionName = args[0];

	if (optionName == L"current device") {
		if (!requestedSource.has_value()) {
			return;
		}

		if (args.size() < 2) {
			logHelpers.generic.log(L"resolve: current device: second argument is required");
			setInvalid(true);
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

		if (di.state != CaptureManager::State::eOK) {
			return;
		}

		if (deviceProperty == L"type") {
			resolveBufferString = di.type == wasapi_wrappers::MediaDeviceType::eINPUT ? L"input" : L"output";
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
			setInvalid(true);
		}

		return;
	}

	if (optionName == L"deviceList") {
		auto& wrapper = helper.getSnapshot().deviceListWrapper;
		auto lock = wrapper.getLock();
		resolveBufferString = wrapper.list;
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

		auto map = option_parsing::Option{ args[1] }.asMap(L'|', L' ');

		auto procName = map.get(L"proc").asIString();
		const auto channelName = map.get(L"channel").asIString();
		const auto handlerName = map.get(L"handlerName").asIString();

		if (channelName.empty()) {
			if (optionName == L"value") {
				logHelpers.generic.log(L"Resolve: value: channel is required");
			} else {
				logHelpers.generic.log(L"Resolve handlerInfo: channel is required");
			}
			setInvalid(true);
			return;
		}

		if (handlerName.empty()) {
			if (optionName == L"value") {
				logHelpers.generic.log(L"Resolve: value: handlerName is required");
			} else {
				logHelpers.generic.log(L"Resolve handlerInfo: handlerName is required");
			}
			setInvalid(true);
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
			if (!checkHandlerShouldExist(procName, channelOpt.value(), handlerName)) {
				// handlerInfo has its own check for isHandlerShouldExist
				// inside #resolveProp()
				return;
			}

			index ind;
			try {
				ind = parser.parse(map, L"index").valueOr(0);
			} catch (option_parsing::OptionParser::Exception&) {
				logHelpers.generic.log(L"resolve value: can't parse index");
				setInvalid(true);
				return;
			}

			buffer_printer::BufferPrinter bp;
			try {
				const auto value = getValue(procName, handlerName, channelOpt.value(), ind);
				bp.print(value);
			} catch (InvalidIndexException&) {
				logger.error(L"fatal error: section variable requested value with out of bounds index");
				// measure was invalidated in #getValue
			}
			resolveBufferString = bp.getBufferView();
			return;
		}
		if (optionName == L"handlerInfo") {
			auto propName = map.get(L"data").asIString();
			if (propName.empty()) {
				logHelpers.generic.log(L"handlerInfo must have 'data' specified");
				setInvalid(true);
				return;
			}
			resolveProp(resolveBufferString, procName, channelOpt.value(), handlerName, propName);
			return;
		}
		return;
	}

	logHelpers.unknownSectionVariable.log(optionName);
	setInvalid(true);
}

double AudioParent::getValue(isview unitName, isview handlerName, Channel channel, index ind) {
	if (!helper.getSnapshot().deviceIsAvailable) {
		return 0.0;
	}

	auto& data = helper.getSnapshot().data;
	auto lock = data.getLock();

	auto procIter = data._.find(unitName);
	if (procIter == data._.end()) {
		return 0.0;
	}

	const auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channel);
	if (channelSnapshotIter == processingSnapshot.end()) {
		return 0.0;
	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(handlerName);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		return 0.0;
	}

	auto& values = handlerSnapshotIter->second.values;
	const auto layersCount = values.getBuffersCount();
	if (layersCount == 0) {
		logger.error(L"{}: {}: value was requested but handler doesn't have values", unitName, handlerName);
		setInvalid(true);
		throw InvalidIndexException{};
	}
	const auto valuesCount = values.getBufferSize();
	if (ind >= valuesCount) {
		logger.error(L"{}: {}: there are {} values but index {} was requested", unitName, handlerName, valuesCount, ind);
		setInvalid(true);
		throw InvalidIndexException{};
	}

	return static_cast<double>(values[0][ind]);
}

bool AudioParent::checkHandlerShouldExist(isview procName, Channel channel, isview handlerName) const {
	const auto procDataIter = paramHelper.getParseResult().find(procName);
	if (procDataIter == paramHelper.getParseResult().end()) {
		logHelpers.processingNotFound.log(procName);
		return false;
	}

	auto procData = procDataIter->second;
	array_view<Channel> channels = procData.channels;
	if (!channels.contains(channel)) {
		logHelpers.processingDoesNotHaveChannel.log(procName, channel);
		return false;
	}

	auto& handlerMap = procData.handlers;
	if (handlerMap.find(handlerName) == handlerMap.end()) {
		return false;
	}

	return true;
}

rxtd::isview AudioParent::findProcessingFor(isview handlerName) const {
	for (auto& [name, pd] : paramHelper.getParseResult()) {
		auto& order = pd.handlerOrder;
		if (std::find(order.begin(), order.end(), handlerName) != order.end()) {
			return name;
		}
	}

	logHelpers.noProcessingHaveHandler.log(handlerName);

	return {};
}

void AudioParent::initLogHelpers() {
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
	logHelpers.processingDoesNotHaveChannel.setLogFunction(
		[](Logger& logger, istring procName, Channel channel) {
			logger.error(L"Processing {} doesn't have channel {}", procName, ChannelUtils::getHumanName(channel));
		}
	);
	logHelpers.handlerDoesNotHaveProps.setLogFunction(
		[](Logger& logger, istring type) {
			logger.error(L"handler type {} doesn't have any props", type);
		}
	);
	logHelpers.propNotFound.setLogFunction(
		[](Logger& logger, istring type, istring propName) {
			logger.error(L"Handler type {} doesn't have info '{}'", type, propName);
		}
	);
}

void AudioParent::runFinisher(
	handler::ExternalMethods::FinishMethodType finisher, const handler::HandlerBase::ExternalData& handlerData, isview procName, Channel channel, isview handlerName
) const {
	handler::ExternalMethods::CallContext context{
		version,
		ChannelUtils::getTechnicalName(channel),
		L"",
		bufferPrinter,
		parser
	};

	bufferPrinter.print(L"{}-{}-{}", procName, handlerName, context.channelName);
	string filePrefix = string{ bufferPrinter.getBufferView() };
	context.filePrefix = filePrefix;

	finisher(handlerData, context);
}

AudioParent::DeviceRequest AudioParent::readRequest() const {
	CaptureManager::SourceDesc result;

	using ST = CaptureManager::SourceDesc::Type;

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

	return result;
}

void AudioParent::resolveProp(
	string& resolveBufferString,
	isview procName, Channel channel, isview handlerName, isview propName
) {
	if (!checkHandlerShouldExist(procName, channel, handlerName)) {
		return;
	}

	const auto procIter0 = paramHelper.getParseResult().find(procName);
	const auto handlerInfoIter = procIter0->second.handlers.find(handlerName);

	const HandlerInfo* const handlerInfo = &handlerInfoIter->second;
	const auto propGetter = handlerInfo->meta.externalMethods.getProp;

	if (propGetter == nullptr) {
		logHelpers.handlerDoesNotHaveProps.log(handlerInfo->type);
		return;
	}

	auto& data = helper.getSnapshot().data;
	auto lock = data.getLock();

	// "not found" errors below are not logged because we have already checked everything above,
	// and if we still don't find requested info then it is caused either by delay in updating second thread
	// or by device not having requested channel

	handler::HandlerBase::ExternalData const* handlerExternalData = nullptr;

	auto findExternalData = [&](const ProcessingOrchestrator::Snapshot& snap) -> handler::HandlerBase::ExternalData const* {
		if (auto procIter = snap.find(procName);
			procIter != snap.end()) {

			auto& processingSnapshot = procIter->second;

			if (auto channelSnapshotIter = processingSnapshot.find(channel);
				channelSnapshotIter != processingSnapshot.end()) {
				auto& channelSnapshot = channelSnapshotIter->second;

				if (auto handlerSnapshotIter = channelSnapshot.find(handlerName);
					handlerSnapshotIter != channelSnapshot.end()) {
					return &handlerSnapshotIter->second.handlerSpecificData;
				}
			}
		}

		return nullptr;
	};

	handlerExternalData = findExternalData(data._);

	if (handlerExternalData == nullptr) {
		handlerExternalData = findExternalData(clearSnapshot);
	}

	if (handlerExternalData == nullptr) {
		// we should never get here
		// but just it case there is an error, let's make sure that plugin won't crash the application
		logHelpers.generic.log(L"unexpected (handlerSpecificData == nullptr), tell the developer about this error");
		return;
	}

	handler::ExternalMethods::CallContext context{
		version,
		ChannelUtils::getTechnicalName(channel),
		L"",
		bufferPrinter,
		parser
	};

	bufferPrinter.print(L"{}-{}-{}", procName, handlerName, context.channelName);
	string filePrefix = string{ bufferPrinter.getBufferView() };
	context.filePrefix = filePrefix;

	try {
		const bool found = propGetter(*handlerExternalData, propName, context);
		if (!found) {
			logHelpers.propNotFound.log(handlerInfo->type, propName);
			return;
		}
	} catch (option_parsing::OptionParser::Exception&) {
		// todo make log message more informative
		logHelpers.generic.log(L"resolve handlerInfo: invalid request, can't parse number");
		return;
	}

	resolveBufferString = context.printer.getBufferView();
}

void AudioParent::runFinishers(ProcessingOrchestrator::Snapshot& snapshot) const {
	for (const auto& [procName, procInfo] : paramHelper.getParseResult()) {
		auto procIter = snapshot.find(procName);
		if (procIter == snapshot.end()) { continue; }
		ProcessingManager::Snapshot& procSnapshot = procIter->second;

		for (const auto channel : procInfo.channels) {
			auto channelIter = procSnapshot.find(channel);
			if (channelIter == procSnapshot.end()) { continue; }
			ProcessingManager::ChannelSnapshot& channelSnapshot = channelIter->second;

			for (const auto& [handlerName, handlerInfo] : procInfo.handlers) {
				auto handlerIter = channelSnapshot.find(handlerName);
				if (handlerIter == channelSnapshot.end()) { continue; }
				handler::HandlerBase::Snapshot& handlerSnapshot = handlerIter->second;

				const handler::ExternalMethods::FinishMethodType finisher = handlerInfo.meta.externalMethods.finish;
				runFinisher(
					finisher, handlerSnapshot.handlerSpecificData, procName, channel,
					handlerName
				);
			}
		}
	}
}
