/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParamParser.h"

#include "rxtd/option-parsing/OptionList.h"
#include "rxtd/option-parsing/OptionMap.h"

using namespace std::string_literals;

using namespace rxtd::audio_analyzer;

bool ParamParser::parse(Version _version, bool suppressLogger) {
	anythingChanged = false;
	version = _version;

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
	hch.setVersion(version);
	handlerNames.clear();

	ProcessingsInfoMap result;
	for (const auto& nameOption : processingIndices) {
		if (!checkNameAllowed(nameOption.asString())) {
			logger.error(L"Processing '{}': invalid processing name");
			anythingChanged = true;
			return {};
		}

		const auto name = nameOption.asIString() % own();
		auto data = parseProcessing(name % csView(), logger.context(L"Processing {}: ", name), std::move(parseResult[name]));
		if (data.channels.empty() || data.handlerOrder.empty()) {
			continue;
		}

		result[name] = std::move(data);
	}

	parseResult = std::move(result);

	return anythingChanged || hch.isAnythingChanged();
}

ProcessingData ParamParser::parseProcessing(sview name, Logger cl, ProcessingData data) {
	string processingOptionIndex = L"Processing-"s += name;
	auto processingDescriptionOption = rain.read(processingOptionIndex);
	if (processingDescriptionOption.empty()) {
		processingOptionIndex = L"Processing_"s += name;
		processingDescriptionOption = rain.read(processingOptionIndex);
	}

	if (processingDescriptionOption.empty()) {
		cl.error(L"processing description not found");
		anythingChanged = true;
		return {};
	}

	auto processingMap = processingDescriptionOption.asMap(L'|', L' ');

	auto channelsList = processingMap.get(L"channels").asList(L',');
	if (channelsList.empty()) {
		cl.error(L"channels not found");
		anythingChanged = true;
		return {};
	}
	auto channels = parseChannels(channelsList, cl);
	if (channels != data.channels) {
		anythingChanged = true;
	}
	data.channels = std::move(channels);
	if (data.channels.empty()) {
		cl.error(L"no valid channels found");
		return {};
	}

	auto handlersOption = processingMap.get(L"handlers");
	if (handlersOption.empty()) {
		cl.error(L"handlers not found");
		anythingChanged = true;
		return {};
	}

	auto handlersList = handlersOption.asList(L',');
	if (!checkListUnique(handlersList)) {
		cl.error(L"found repeating handlers, invalidate processing");
		anythingChanged = true;
		return {};
	}

	auto handlersOK = parseHandlers(handlersList, data, cl);
	if (!handlersOK) {
		// #parseHandlers must have logged all errors in this case
		anythingChanged = true;
		return {};
	}

	parseFilter(processingMap, data.filter, cl);
	parseTargetRate(processingMap, data, cl);

	if (unusedOptionsWarning) {
		const auto untouched = processingMap.getListOfUntouched();
		if (!untouched.empty()) {
			cl.warning(L"unused options: {}", untouched);
		}
	}

	return data;
}

void ParamParser::parseFilter(const OptionMap& optionMap, ProcessingData::FilterInfo& fi, Logger& cl) const {
	const auto filterDescription = optionMap.get(L"filter");

	if (filterDescription.asString() == fi.raw) {
		return;
	}

	anythingChanged = true;
	fi.raw = filterDescription.asString();

	const auto [filterTypeOpt, filterParams] = filterDescription.breakFirst(L' ');

	const auto filterType = filterTypeOpt.asIString(L"none");
	auto filterLogger = cl.context(L"filter: ");

	if (filterType == L"none") {
		fi.creator = {};
		return;
	}

	if (filterType == L"like-a") {
		fi.creator = audio_utils::FilterCascadeParser::parse(
			common::options::Option{
				L"bqHighPass[q 0.3, freq 200, forcedGain 3.58]  " // spaces in the ends of the strings are necessary
				L"bwLowPass[order 5, freq 10000] "
			}, filterLogger
		);
		return;
	}

	if (filterType == L"like-d") {
		fi.creator = audio_utils::FilterCascadeParser::parse(
			common::options::Option{
				L"bqHighPass[q 0.3, freq 200, forcedGain 3.65]  " // spaces in the ends of the strings are necessary
				L"bqPeak[q 1.0, freq 6000, gain 5.28] "
				L"bwLowPass[order 5, freq 10000] "
			}, filterLogger
		);
		return;
	}

	if (filterType == L"custom") {
		fi.creator = audio_utils::FilterCascadeParser::parse(filterParams, filterLogger);
		return;
	}

	filterLogger.error(L"filter class '{}' is not supported", filterType);
	fi.creator = {};
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
		auto [iter, inserted] = set.insert(option.asIString());
		if (!inserted) {
			return false;
		}
	}
	return true;
}

bool ParamParser::checkNameAllowed(sview name) {
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

std::vector<Channel> ParamParser::parseChannels(const OptionList& channelsStringList, Logger& logger) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = ChannelUtils::parse(channelOption.asIString());
		if (!opt.has_value()) {
			logger.error(L"can't parse '{}' as channel", channelOption.asString());
			continue;
		}
		set.insert(opt.value());
	}

	std::vector<Channel> result;
	for (auto c : set) {
		result.push_back(c);
	}
	return result;
}

bool ParamParser::parseHandlers(const OptionList& names, ProcessingData& data, const Logger& cl) {
	auto oldOrder = std::exchange(data.handlerOrder, {});

	for (auto nameOption : names) {
		if (!checkNameAllowed(nameOption.asString())) {
			cl.error(L"handler '{}': invalid handler name", nameOption);
			anythingChanged = true;
			return {};
		}

		auto name = istring{ nameOption.asIString() };
		if (auto [iter, isNewElement] = handlerNames.insert(name);
			!isNewElement) {
			cl.error(L"handler {} was already used in another processing, invalidate processing", name);
			return {};
		}

		auto handlerInfo = hch.getHandlerInfo(name, cl.context(L"handler {}: ", name));

		if (handlerInfo == nullptr) {
			cl.error(L"invalidate processing", name);
			return {};
		}

		data.handlers[name] = *handlerInfo;
		data.handlerOrder.push_back(name);
	}

	if (data.handlerOrder.empty()) {
		cl.warning(L"no valid handlers found");
		anythingChanged = true;
		return {};
	}

	return true;
}
