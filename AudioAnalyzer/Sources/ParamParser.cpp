/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParamParser.h"

#include "option-parsing/OptionMap.h"
#include "option-parsing/OptionList.h"

using namespace std::string_literals;

using namespace audio_analyzer;

bool ParamParser::parse(index _legacyNumber, bool suppressLogger) {
	anythingChanged = false;
	legacyNumber = _legacyNumber;

	auto logger = suppressLogger ? Logger::getSilent() : rain.createLogger();

	auto defaultTargetRate = rain.read(L"TargetRate").asInt(44100);
	if (defaultTargetRate < 0) {
		logger.warning(L"Invalid TargetRate {}, must be > 0. Assume 0.", defaultTargetRate);
		defaultTargetRate = 0;
	}

	unusedOptionsWarning = rain.read(L"UnusedOptionsWarning").asBool(true);

	auto processingIndices = rain.read(L"Processing").asList(L'|');
	if (!checkListUnique(processingIndices)) {
		logger.error(L"Found repeating processings, aborting");
		anythingChanged = true;
		parseResult = {};
	}

	hch.reset();
	hch.setUnusedOptionsWarning(unusedOptionsWarning);
	hch.setLegacyNumber(legacyNumber);

	ProcessingsInfoMap result;
	for (const auto& nameOption : processingIndices) {
		const auto name = nameOption.asIString() % own();

		auto oldData = std::move(parseResult[name]);
		parseProcessing(name % csView(), logger.context(L"Processing {}: ", name), oldData);
		if (oldData.channels.empty() || oldData.handlersInfo.order.empty()) {
			continue;
		}

		result[name] = std::move(oldData);
	}

	parseResult = std::move(result);

	return anythingChanged || hch.isAnythingChanged();
}

void ParamParser::parseProcessing(sview name, Logger cl, ProcessingData& oldData) {
	string processingOptionIndex = L"Processing-"s += name;
	auto processingDescriptionOption = rain.read(processingOptionIndex);
	if (processingDescriptionOption.empty()) {
		processingOptionIndex = L"Processing_"s += name;
		processingDescriptionOption = rain.read(processingOptionIndex);
	}

	oldData.finishers.clear();

	if (processingDescriptionOption.empty()) {
		cl.error(L"processing description not found");
		oldData = {};
		anythingChanged = true;
		return;
	}

	auto processingMap = processingDescriptionOption.asMap(L'|', L' ');

	auto channelsList = processingMap.get(L"channels").asList(L',');
	if (channelsList.empty()) {
		cl.error(L"channels not found");
		oldData.channels = {};
		anythingChanged = true;
		return;
	}
	auto channels = parseChannels(channelsList, cl);
	if (channels != oldData.channels) {
		anythingChanged = true;
	}
	oldData.channels = std::move(channels);
	if (oldData.channels.empty()) {
		cl.error(L"no valid channels found");
		return;
	}

	auto handlersOption = processingMap.get(L"handlers");
	if (handlersOption.empty()) {
		cl.error(L"handlers not found");
		oldData.handlersInfo = {};
		anythingChanged = true;
		return;
	}

	auto handlersList = handlersOption.asList(L',');
	if (!checkListUnique(handlersList)) {
		cl.error(L"found repeating handlers, invalidate processing");
		oldData.handlersInfo = {};
		anythingChanged = true;
		return;
	}

	oldData.handlersInfo = parseHandlers(handlersList);
	if (oldData.handlersInfo.order.empty()) {
		cl.warning(L"no valid handlers found");
		anythingChanged = true;
		return;
	}

	for (const auto& [name, info] : oldData.handlersInfo.patchers) {
		if (info.externalMethods.finish != nullptr) {
			oldData.finishers[name] = info.externalMethods.finish;
		}
	}

	parseFilters(processingMap, oldData, cl);
	parseTargetRate(processingMap, oldData, cl);

	if (unusedOptionsWarning) {
		const auto untouched = processingMap.getListOfUntouched();
		if (!untouched.empty()) {
			cl.warning(L"unused options: {}", untouched);
		}
	}
}

void ParamParser::parseFilters(const OptionMap& optionMap, ProcessingData& data, Logger& cl) const {
	const auto filterDescription = optionMap.get(L"filter");

	if (filterDescription.asString() == data.rawFccDescription) {
		return;
	}

	anythingChanged = true;
	data.rawFccDescription = filterDescription.asString();

	const auto [filterTypeOpt, filterParams] = filterDescription.breakFirst(L' ');

	const auto filterType = filterTypeOpt.asIString(L"none");
	auto filterLogger = cl.context(L"filter: ");

	if (filterType == L"none") {
		data.fcc = {};
		return;
	}

	if (filterType == L"like-a") {
		data.fcc = audio_utils::FilterCascadeParser::parse(
			common::options::Option{
				L"bqHighPass[q 0.3, freq 200, forcedGain 3.58]  " // spaces in the ends of the strings are necessary
				L"bwLowPass[order 5, freq 10000] "
			}, filterLogger
		);
		return;
	}

	if (filterType == L"like-d") {
		data.fcc = audio_utils::FilterCascadeParser::parse(
			common::options::Option{
				L"bqHighPass[q 0.3, freq 200, forcedGain 3.65]  " // spaces in the ends of the strings are necessary
				L"bqPeak[q 1.0, freq 6000, gain 5.28] "
				L"bwLowPass[order 5, freq 10000] "
			}, filterLogger
		);
		return;
	}

	if (filterType == L"custom") {
		data.fcc = audio_utils::FilterCascadeParser::parse(filterParams, filterLogger);
		return;
	}

	filterLogger.error(L"filter class '{}' is not supported", filterType);
	data.fcc = {};
}

void ParamParser::parseTargetRate(const OptionMap& optionMap, ProcessingData& data, Logger& cl) const {
	const auto targetRate = optionMap.get(L"targetRate").asInt(defaultTargetRate);
	if (targetRate == data.targetRate) {
		return;
	}

	anythingChanged = true;
	data.targetRate = targetRate;
}

bool ParamParser::checkListUnique(const OptionList& list) {
	std::set<isview> set;
	for (auto option : list) {
		auto view = option.asIString();
		if (set.find(view) != set.end()) {
			return false;
		}
		set.insert(view);
	}
	return true;
}

std::set<Channel> ParamParser::parseChannels(const OptionList& channelsStringList, Logger& logger) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = ChannelUtils::parse(channelOption.asIString());
		if (!opt.has_value()) {
			logger.error(L"can't parse '{}' as channel", channelOption.asString());
			continue;
		}
		set.insert(opt.value());
	}

	return set;
}

ParamParser::HandlerPatchersInfo
ParamParser::parseHandlers(const OptionList& names) {
	HandlerPatchersInfo result;

	for (auto nameOption : names) {
		auto name = istring{ nameOption.asIString() };

		auto handler = hch.getHandler(name);

		if (handler == nullptr) {
			if (legacyNumber < 104) {
				continue;
			} else {
				return result;
			}
		}

		result.patchers[name] = *handler;
		result.order.push_back(name);
	}

	return result;
}
