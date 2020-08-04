/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParamParser.h"

#include "sound-processing/sound-handlers/BlockHandler.h"
#include "sound-processing/sound-handlers/WaveForm.h"
#include "sound-processing/sound-handlers/Loudness.h"

#include "sound-processing/sound-handlers/spectrum-stack/FftAnalyzer.h"
#include "sound-processing/sound-handlers/spectrum-stack/BandResampler.h"
#include "sound-processing/sound-handlers/spectrum-stack/BandCascadeTransformer.h"
#include "sound-processing/sound-handlers/spectrum-stack/WeightedBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_FiniteTimeFilter.h"
#include "sound-processing/sound-handlers/spectrum-stack/UniformBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_LogarithmicValueMapper.h"
#include "sound-processing/sound-handlers/spectrum-stack/Spectrogram.h"
#include "sound-processing/sound-handlers/spectrum-stack/SingleValueTransformer.h"

#include "option-parser/OptionMap.h"
#include "option-parser/OptionList.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool ParamParser::parse() {
	anythingChanged = false;
	legacyNumber = rain.read(L"LegacyNumber").asInt(0);

	auto& logger = rain.getLogger();

	auto defaultTargetRate = rain.read(L"TargetRate").asInt(44100);
	if (defaultTargetRate < 0) {
		logger.warning(L"Invalid TargetRate {}, must be > 0. Assume 0.", defaultTargetRate);
		defaultTargetRate = 0;
	}

	unusedOptionsWarning = rain.read(L"UnusedOptionsWarning").asBool(true);

	auto processingIndices = rain.read(L"Processing").asList(L'|');
	if (!checkListUnique(processingIndices)) {
		rain.getLogger().error(L"Found repeating processings, aborting");
		anythingChanged = true;
		parseResult = { };
	}

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

	return anythingChanged;
}

void ParamParser::parseProcessing(sview name, Logger cl, ProcessingData& oldData) const {
	string processingOptionIndex = L"Processing-"s += name;
	auto processingDescriptionOption = rain.read(processingOptionIndex);
	if (processingDescriptionOption.empty()) {
		processingOptionIndex = L"Processing_"s += name;
		processingDescriptionOption = rain.read(processingOptionIndex);
	}

	if (processingDescriptionOption.empty()) {
		cl.error(L"processing description not found");
		oldData = { };
		anythingChanged = true;
		return;
	}

	auto processingMap = processingDescriptionOption.asMap(L'|', L' ');

	auto channelsList = processingMap.get(L"channels"sv).asList(L',');
	if (channelsList.empty()) {
		cl.error(L"channels not found");
		oldData.channels = { };
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

	auto handlersOption = processingMap.get(L"handlers"sv);
	if (handlersOption.empty()) {
		cl.error(L"handlers not found");
		oldData.handlersInfo = { };
		anythingChanged = true;
		return;
	}

	auto handlersList = handlersOption.asList(L',');
	if (!checkListUnique(handlersList)) {
		cl.error(L"found repeating handlers, invalidate processing");
		oldData.handlersInfo = { };
		anythingChanged = true;
		return;
	}

	oldData.handlersInfo = parseHandlers(handlersList, std::move(oldData.handlersInfo));
	if (oldData.handlersInfo.map.empty()) {
		cl.warning(L"no valid handlers found");
		anythingChanged = true;
		return;
	}

	if (const auto granularity = std::max(processingMap.get(L"granularity").asFloat(10.0), 1.0) / 1000.0;
		granularity != oldData.granularity) {
		anythingChanged = true;
		oldData.granularity = granularity;
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

void ParamParser::parseFilters(const utils::OptionMap& optionMap, ProcessingData& data, Logger& cl) const {
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
		data.fcc = { };
		return;
	}

	if (legacyNumber < 104) {
		filterLogger.error(L"filter class '{}' is not supported", filterType);
		data.fcc = { };
	}

	if (filterType == L"replayGain") {
		data.fcc = audio_utils::FilterCascadeParser::parse(
			utils::Option{
				L"bqHighPass[q 0.8, freq 160, forcedGain 3.65] " // spaces in the ends of the strings are necessary
				L"bqPeak[q 3.0, freq 1125, gain -3.8] "
				L"bqPeak[q 3.0, freq 3665, gain 5.28] "
				L"bwLowPass[order 5, freq 10000] "
			}, filterLogger
		);
		return;
	}

	if (filterType == L"rxtd") {
		data.fcc = audio_utils::FilterCascadeParser::parse(
			utils::Option{
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
	data.fcc = { };
}

void ParamParser::parseTargetRate(const utils::OptionMap& optionMap, ProcessingData& data, Logger& cl) const {
	const auto targetRate = optionMap.get(L"targetRate").asInt(defaultTargetRate);
	if (targetRate == data.targetRate) {
		return;
	}

	anythingChanged = true;
	data.targetRate = targetRate;
}

bool ParamParser::checkListUnique(const utils::OptionList& list) {
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

std::set<Channel> ParamParser::parseChannels(const utils::OptionList& channelsStringList, Logger& logger) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = Channel::channelParser.find(channelOption.asIString());
		if (!opt.has_value()) {
			logger.error(L"can't parse '{}' as channel", channelOption.asString());
			continue;
		}
		set.insert(opt.value());
	}

	return set;
}

ParamParser::HandlerPatcherInfo
ParamParser::parseHandlers(const utils::OptionList& names, HandlerPatcherInfo oldHandlers) const {
	HandlerPatcherInfo result;

	for (auto nameOption : names) {
		auto handler = std::move(oldHandlers.map[nameOption.asIString() % own()]);

		const auto success = parseHandler(nameOption.asString(), result, handler);
		if (!success) {
			if (legacyNumber < 104) {
				continue;
			} else {
				return result;
			}
		}

		result.map[nameOption.asIString() % own()] = std::move(handler);
		result.order.push_back(nameOption.asIString() % own());
	}

	return result;
}

bool ParamParser::parseHandler(sview name, const HandlerPatcherInfo& prevHandlers, HandlerInfo& handler) const {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);
	if (descriptionOption.empty()) {
		optionName = L"Handler_"s += name;
		descriptionOption = rain.read(optionName);
	}

	auto cl = rain.getLogger().context(L"Handler '{}': ", name);

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		return false;
	}

	utils::OptionMap optionMap = descriptionOption.asMap(L'|', L' ');
	string rawDescription2;
	readRawDescription2(optionMap.get(L"type").asIString(), optionMap, rawDescription2);

	if (handler.rawDescription == descriptionOption.asString() && handler.rawDescription2 == rawDescription2) {
		return true;
	}
	anythingChanged = true;

	handler.patcher = getHandlerPatcher(optionMap, cl, prevHandlers);
	if (handler.patcher == nullptr) {
		return false;
	}

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: '{}'", unusedOptions);
	}

	handler.rawDescription2 = std::move(rawDescription2);
	handler.rawDescription = descriptionOption.asString();

	return true;
}

std::shared_ptr<HandlerPatcher> ParamParser::getHandlerPatcher(
	const utils::OptionMap& optionMap,
	Logger& cl,
	const HandlerPatcherInfo& prevHandlers
) const {
	const auto type = optionMap.get(L"type"sv).asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		return nullptr;
	}

	// source must be checked to prevent loops
	const auto source = optionMap.getUntouched(L"source").asIString();
	if (!source.empty()) {
		const bool found = prevHandlers.map.find(source) != prevHandlers.map.end();
		if (!found) {
			cl.error(L"reverse or unknown dependency '{}'", source);
			return nullptr;
		}
	}

	if (type == L"rms") {
		return createPatcher<BlockRms>(optionMap, cl);
	}
	if (type == L"peak") {
		return createPatcher<BlockPeak>(optionMap, cl);
	}
	if (type == L"fft") {
		return createPatcher<FftAnalyzer>(optionMap, cl);
	}
	if (type == L"BandResampler") {
		return createPatcher<BandResampler>(optionMap, cl);
	}
	if (type == L"BandCascadeTransformer") {
		return createPatcher<BandCascadeTransformer>(optionMap, cl);
	}
	if (type == L"WeightedBlur") {
		return createPatcher<WeightedBlur>(optionMap, cl);
	}
	if (type == L"UniformBlur") {
		return createPatcher<UniformBlur>(optionMap, cl);
	}
	if (type == L"spectrogram") {
		return createPatcher<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform") {
		return createPatcher<WaveForm>(optionMap, cl);
	}
	if (type == L"loudness") {
		return createPatcher<Loudness>(optionMap, cl);
	}

	if (legacyNumber >= 104) {
		if (type == L"ValueTransformer") {
			return createPatcher<SingleValueTransformer>(optionMap, cl);
		}
	}

	if (legacyNumber < 104) {
		if (type == L"FiniteTimeFilter") {
			return createPatcher<legacy_FiniteTimeFilter>(optionMap, cl);
		}
		if (type == L"LogarithmicValueMapper") {
			return createPatcher<legacy_LogarithmicValueMapper>(optionMap, cl);
		}
	}

	cl.error(L"unknown type '{}'", type);
	return nullptr;
}

void ParamParser::readRawDescription2(isview type, const utils::OptionMap& optionMap, string& rawDescription2) const {
	if (type == L"BandResampler") {
		auto freqListIndex = optionMap.getUntouched(L"freqList"sv).asString();
		if (!freqListIndex.empty()) {
			auto freqListOptionName = L"FreqList-"s += freqListIndex;
			auto freqListOption = rain.read(freqListOptionName);
			if (freqListOption.empty()) {
				freqListOptionName = L"FreqList_"s += freqListIndex;
				freqListOption = rain.read(freqListOptionName);
			}
			rawDescription2 = freqListOption.asString();
		}
	}
}
