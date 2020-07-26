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

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

ParamParser::ParamParser(utils::Rainmeter& rain, bool unusedOptionsWarning) :
	rain(rain),
	log(rain.getLogger()),
	unusedOptionsWarning(unusedOptionsWarning) {
}

std::vector<ParamParser::ProcessingData> ParamParser::parse() {
	std::vector<ProcessingData> result;

	auto processingIndices = rain.read(L"Processing").asList(L'|');
	if (!checkListUnique(processingIndices)) {
		rain.getLogger().error(L"Found repeating processings, aborting");
		return result;
	}

	for (const auto& nameOption : processingIndices) {
		const auto name = nameOption.asString();

		auto procOpt = parseProcessing(name);
		if (!procOpt.has_value()) {
			rain.getLogger().error(L"Invalid processing '{}'", name);
			continue;
		}

		result.push_back(procOpt.value());
	}

	return result;
}

std::optional<ParamParser::ProcessingData> ParamParser::parseProcessing(sview name) {
	auto cl = rain.getLogger().context(L"Processing {}: ", name);

	string processingOptionIndex = L"Processing-"s += name;
	auto processingDescriptionOption = rain.read(processingOptionIndex);
	if (processingDescriptionOption.empty()) {
		processingOptionIndex = L"Processing_"s += name;
		processingDescriptionOption = rain.read(processingOptionIndex);
	}

	if (processingDescriptionOption.empty()) {
		cl.error(L"processing description not found");
		return { };
	}

	auto processingMap = processingDescriptionOption.asMap(L'|', L' ');

	auto channelsList = processingMap.get(L"channels"sv).asList(L',');
	if (channelsList.empty()) {
		cl.error(L"channels not found");
		return { };
	}
	auto channels = parseChannels(channelsList);
	if (channels.empty()) {
		cl.error(L"no valid channels found");
		return { };
	}

	auto handlersOption = processingMap.get(L"handlers"sv);
	if (handlersOption.empty()) {
		cl.error(L"handlers not found");
		return { };
	}

	auto handlersList = handlersOption.asList(L',');
	if (!checkListUnique(handlersList)) {
		cl.error(L"found repeating handlers, invalidate processing");
		return { };
	}

	auto handlers = parseHandlers(handlersList);
	if (handlers.empty()) {
		cl.warning(L"no valid handlers found");
		return { };
	}

	index targetRate = processingMap.get(L"targetRate").asInt(44100);
	auto ffc = audio_utils::FilterCascadeParser::parse(processingMap.get(L"filter").asSequence());

	return ProcessingData{
		targetRate,
		std::move(ffc),
		channels,
		handlers
	};
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

std::set<Channel> ParamParser::parseChannels(utils::OptionList channelsStringList) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = Channel::channelParser.find(channelOption.asIString());
		if (!opt.has_value()) {
			log.error(L"Can't parse '{}' as channel", channelOption.asString());
			continue;
		}
		set.insert(opt.value());
	}

	return set;
}

std::vector<ParamParser::HandlerInfo> ParamParser::parseHandlers(const utils::OptionList& names) {
	std::vector<HandlerInfo> result;

	for (auto nameOption : names) {
		auto name = nameOption.asString();
		auto patcher = parseHandler(name, result);
		if (patcher == nullptr) {
			continue;
		}

		HandlerInfo info{ };
		info.name = nameOption.asIString();
		info.patcher = patcher;
		result.push_back(info);
	}

	return result;
}

ParamParser::Patcher ParamParser::parseHandler(sview name, array_view<HandlerInfo> prevHandlers) {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);
	if (descriptionOption.empty()) {
		optionName = L"Handler_"s += name;
		descriptionOption = rain.read(optionName);
	}

	auto cl = rain.getLogger().context(L"Handler '{}': ", name);

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		return { };
	}

	auto optionMap = descriptionOption.asMap(L'|', L' ');

	auto patcher = getHandlerPatcher(optionMap, cl, prevHandlers);
	if (patcher == nullptr) {
		return { };
	}

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: '{}'", unusedOptions);
	}

	return patcher;
}

ParamParser::Patcher ParamParser::getHandlerPatcher(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl,
	array_view<HandlerInfo> prevHandlers
) {
	const auto type = optionMap.get(L"type"sv).asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		return nullptr;
	}

	// source must be checked to prevent loops
	const auto source = optionMap.getUntouched(L"source").asIString();
	if (!source.empty()) {
		bool found = false;
		for (auto& info : prevHandlers) {
			if (info.name == source) {
				found = true;
				break;
			}
		}
		if (!found) {
			cl.error(L"reverse or unknown dependency '{}'", source);
			return nullptr;
		}
	}

	if (type == L"rms") {
		return parseHandlerT<BlockRms>(optionMap, cl);
	}
	if (type == L"peak") {
		return parseHandlerT<BlockPeak>(optionMap, cl);
	}
	if (type == L"fft") {
		return parseHandlerT<FftAnalyzer>(optionMap, cl);
	}
	if (type == L"BandResampler") {
		return parseHandlerT2<BandResampler>(optionMap, cl);
	}
	if (type == L"BandCascadeTransformer") {
		return parseHandlerT<BandCascadeTransformer>(optionMap, cl);
	}
	if (type == L"WeightedBlur") {
		return parseHandlerT<WeightedBlur>(optionMap, cl);
	}
	if (type == L"UniformBlur") {
		return parseHandlerT<UniformBlur>(optionMap, cl);
	}
	if (type == L"FiniteTimeFilter") {
		return parseHandlerT<legacy_FiniteTimeFilter>(optionMap, cl);
	}
	if (type == L"LogarithmicValueMapper") {
		return parseHandlerT<legacy_LogarithmicValueMapper>(optionMap, cl);
	}
	if (type == L"spectrogram") {
		return parseHandlerT2<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform") {
		return parseHandlerT2<WaveForm>(optionMap, cl);
	}
	if (type == L"loudness") {
		return parseHandlerT<Loudness>(optionMap, cl);
	}
	if (type == L"ValueTransformer") {
		return parseHandlerT<SingleValueTransformer>(optionMap, cl);
	}

	cl.error(L"unknown type '{}'", type);
	return nullptr;
}
