// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "ParamHelper.h"

#include "rxtd/option_parsing/Option.h"

using namespace std::string_literals;

using rxtd::audio_analyzer::options::ParamHelper;
using rxtd::audio_analyzer::options::ProcessingData;
using rxtd::audio_analyzer::Channel;
using rxtd::filter_utils::FilterCascadeParser;

bool ParamHelper::readOptions(Version _version) {
	version = _version;

	auto logger = rain.createLogger();
	parser.setLogger(logger);

	auto defaultTargetRate = parser.parse(rain.read(L"TargetRate"), L"TargetRate").valueOr(44100);
	if (defaultTargetRate < 0) {
		logger.warning(L"Invalid TargetRate {}, must be > 0. Assume 0.", defaultTargetRate);
		defaultTargetRate = 0;
	}

	unusedOptionsWarning = parser.parse(rain.read(L"LogUnusedOptions"), L"LogUnusedOptions").valueOr(true);

	auto processingIndices = rain.read(L"ProcessingUnits").asList(L',');
	if (!checkListUnique(processingIndices)) {
		logger.error(L"Found repeating processing units, aborting");
		return true;
	}

	hch.reset();
	hch.setUnusedOptionsWarning(unusedOptionsWarning);
	hch.setVersion(version);
	handlerNames.clear();

	bool anyChanges = false;

	ProcessingsInfoMap result;
	for (const auto& nameOption : processingIndices) {
		if (!checkNameAllowed(nameOption.asString())) {
			logger.error(L"Invalid processing unit name: {}");
			throw InvalidOptionsException{};
		}

		const auto name = nameOption.asIString() % own();
		auto& procParams = parseResult[name];
		anyChanges |= parseProcessing(name % csView(), logger.context(L"{}: ", name), procParams);

		result[name] = std::move(procParams);
	}

	parseResult = std::move(result);

	return anyChanges;
}

bool ParamHelper::parseProcessing(sview name, Logger cl, ProcessingData& data) const {
	string processingOptionIndex = L"unit-"s += name;
	auto processingDescriptionOption = rain.read(processingOptionIndex);

	if (processingDescriptionOption.empty()) {
		cl.error(L"unit description is not found");
		throw InvalidOptionsException{};
	}

	auto processingMap = processingDescriptionOption.asMap(L'|', L' ');

	auto channelsList = processingMap.get(L"channels").asList(L',');
	if (channelsList.empty()) {
		cl.error(L"channels not found");
		throw InvalidOptionsException{};
	}

	bool anyChanges = false;

	auto channels = parseChannels(channelsList, cl);
	if (channels != data.channels) {
		anyChanges = true;
	}
	data.channels = std::move(channels);
	if (data.channels.empty()) {
		cl.error(L"no valid channels found");
		throw InvalidOptionsException{};
	}

	auto handlersOption = processingMap.get(L"handlers");
	if (handlersOption.empty()) {
		cl.error(L"handlers not found");
		throw InvalidOptionsException{};
	}

	if (data.handlersRaw != handlersOption.asIString()) {
		anyChanges = true;
	}
	data.handlersRaw = handlersOption.asIString();

	auto handlersList = handlersOption.asSequence(L'(', L')', L',', false, cl);
	{
		std::set<isview> handlerNames;
		for (auto [nameOpt,_, _2] : handlersList) {
			auto [iter, inserted] = handlerNames.insert(nameOpt.asIString());
			if (!inserted) {
				cl.error(L"found repeating handlers, invalidate processing");
				throw InvalidOptionsException{};
			}
		}
	}

	anyChanges |= parseHandlers(handlersList, data, cl);

	anyChanges |= parseFilter(processingMap, data.filter, cl);
	anyChanges |= parseTargetRate(processingMap, data.targetRate, cl);

	if (unusedOptionsWarning) {
		const auto untouched = processingMap.getListOfUntouched();
		if (!untouched.empty()) {
			cl.warning(L"unused options: {}", untouched);
		}
	}

	return anyChanges;
}

bool ParamHelper::parseFilter(const OptionMap& optionMap, ProcessingData::FilterInfo& fi, Logger& cl) const {
	const auto filterDescription = optionMap.get(L"filter");

	if (filterDescription.asString() == fi.raw) {
		return false;
	}

	fi.raw = filterDescription.asString();

	const auto [filterTypeOpt, filterParams] = filterDescription.breakFirst(L' ');

	const auto filterType = filterTypeOpt.asIString(L"none");
	auto filterLogger = cl.context(L"filter: ");

	parser.setLogger(cl);
	FilterCascadeParser fcp{ parser };

	if (filterType == L"none") {
		fi.creator = {};
	} else if (filterType == L"like-a") {
		fi.creator = fcp.parse(
			option_parsing::Option{
				L"bqHighPass(q 0.3, freq 200, forcedGain 3.58),"
				L"bwLowPass(order 5, freq 10000),"
			}, filterLogger
		);
	} else if (filterType == L"like-d") {
		fi.creator = fcp.parse(
			option_parsing::Option{
				L"bqHighPass(q 0.3, freq 200, forcedGain 3.65),"
				L"bqPeak(q 1.0, freq 6000, gain 5.28),"
				L"bwLowPass(order 5, freq 10000),"
			}, filterLogger
		);
	} else if (filterType == L"custom") {
		fi.creator = fcp.parse(filterParams, filterLogger);
	} else {
		filterLogger.error(L"filter class is not supported: {}", filterType);
		fi.creator = {};
	}

	return true;
}

bool ParamHelper::parseTargetRate(const OptionMap& optionMap, index& rate, Logger& cl) const {
	const auto targetRate = parser.parse(optionMap, L"targetRate").valueOr(defaultTargetRate);
	if (targetRate == rate) {
		return false;
	}

	rate = targetRate;
	return true;
}

bool ParamHelper::checkListUnique(const OptionList& list) {
	std::set<isview> set;
	for (auto option : list) {
		auto [iter, inserted] = set.insert(option.asIString());
		if (!inserted) {
			return false;
		}
	}
	return true;
}

bool ParamHelper::checkNameAllowed(sview name) {
	// Names are only allowed to have latin letters, digits and underscore symbol.
	// Symbols are deliberately checked manually
	// to forbid non-latin letters.

	const wchar_t first = name.front();
	const bool firstIsAllowed = (first >= L'a' && first <= L'z')
		|| (first >= L'A' && first <= L'Z') || first == L'_';

	if (!firstIsAllowed) {
		return false;
	}

	return std::all_of(
		name.begin(), name.end(),
		[](wchar_t c) {
			return (c >= L'a' && c <= L'z')
				|| (c >= L'A' && c <= L'Z')
				|| (c >= L'0' && c <= L'9')
				|| c == L'_';
		}
	);
}

std::vector<Channel> ParamHelper::parseChannels(const OptionList& channelsStringList, Logger& logger) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = ChannelUtils::parse(channelOption.asIString());
		if (!opt.has_value()) {
			logger.error(L"can't parse as channel: {}", channelOption.asString());
			throw InvalidOptionsException{};
		}
		set.insert(opt.value());
	}

	std::vector<Channel> result;
	for (auto c : set) {
		result.push_back(c);
	}
	return result;
}

bool ParamHelper::parseHandlers(const OptionSequence& names, ProcessingData& data, const Logger& cl) const {
	auto oldOrder = std::exchange(data.handlerOrder, {});

	bool anyChanges = false;

	for (auto [nameOpt, sourceOpt, postfix] : names) {
		if (!checkNameAllowed(nameOpt.asString())) {
			cl.error(L"invalid handler name: {}", nameOpt);
			throw InvalidOptionsException{};
		}

		auto name = istring{ nameOpt.asIString() };
		if (auto [iter, isNewElement] = handlerNames.insert(name);
			!isNewElement) {
			cl.error(L"invalid unit: handler {} has already been used in another processing unit", name);
			throw InvalidOptionsException{};
		}

		auto& handlerInfo = hch.getHandlerInfo(name, sourceOpt.asIString(), cl.context(L"{}: ", name));

		if (!handlerInfo.valid) {
			if (handlerInfo.changed) {
				cl.error(L"invalidate processing unit", name);
			}
			throw InvalidOptionsException{};
		}

		anyChanges |= handlerInfo.changed;

		data.handlers[name] = handlerInfo.info;
		data.handlerOrder.push_back(name);
	}

	return anyChanges;
}
