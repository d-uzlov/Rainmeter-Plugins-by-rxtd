/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParamParser.h"
#include "sound-processing/sound-handlers/BlockMean.h"
#include "sound-processing/sound-handlers/FftAnalyzer.h"
#include "sound-processing/sound-handlers/Spectrogram.h"
#include "sound-processing/sound-handlers/WaveForm.h"
#include "sound-processing/sound-handlers/BandResampler.h"
#include "sound-processing/sound-handlers/BandCascadeTransformer.h"
#include "sound-processing/sound-handlers/FiniteTimeFilter.h"
#include "sound-processing/sound-handlers/LogarithmicValueMapper.h"
#include "sound-processing/sound-handlers/WeightedBlur.h"
#include "sound-processing/sound-handlers/UniformBlur.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

ParamParser::ParamParser(utils::Rainmeter& rain, bool unusedOptionsWarning) :
	rain(rain),
	log(rain.getLogger()),
	unusedOptionsWarning(unusedOptionsWarning) { }

void ParamParser::parse() {
	handlerPatchersMap.clear();

	utils::OptionParser optionParser { };
	optionParser.setSource(rain.readString(L"Processing"));
	auto processingIndices = optionParser.parse().asList(L'|');
	for (auto processingNameOption : processingIndices) {
		utils::Rainmeter::ContextLogger cl { rain.getLogger() };
		cl.setPrefix(L"Processing {}:", processingNameOption.asString());

		string processingOptionIndex = L"Processing"s;
		processingOptionIndex += L"_";
		processingOptionIndex += processingNameOption.asString();

		utils::OptionParser processingParser;
		processingParser.setSource(rain.readString(processingOptionIndex));
		auto processingMap = processingParser.parse().asMap(L'|', L' ');
		auto channelsOption = processingMap.get(L"channels"sv);
		if (channelsOption.asString().empty()) {
			cl.error(L"channels not found");
			continue;
		}
		auto handlersOption = processingMap.get(L"handlers"sv);
		if (handlersOption.asString().empty()) {
			cl.error(L"handlers not found");
			continue;
		}

		auto channels = parseChannels(channelsOption.asList(L','));
		if (channels.empty()) {
			cl.error(L"no valid channels found");
			continue;
		}
		auto handlersList = handlersOption.asList(L',');

		cacheHandlers(handlersList);

		for (auto channel : channels) {
			for (auto handlerNameOption : handlersList) {
				handlers[channel].emplace_back(handlerNameOption.asIString());
			}
		}
	}
}

const std::map<Channel, std::vector<istring>>& ParamParser::getHandlers() const {
	return handlers;
}

const std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>>&
ParamParser::getPatches() const {
	return handlerPatchersMap;
}

std::set<Channel> ParamParser::parseChannels(utils::OptionList channelsStringList) const {
	std::set<Channel> set;

	for (auto channelOption : channelsStringList) {
		auto opt = Channel::channelParser.find(channelOption.asString());
		if (!opt.has_value()) {
			log.error(L"Can't parse '{}' as channel", channelOption.asString());
			continue;
		}
		set.insert(opt.value());
	}

	return set;
}

void ParamParser::cacheHandlers(const utils::OptionList& indices) {

	for (auto indexOption : indices) {

		auto iter = handlerPatchersMap.lower_bound(indexOption.asIString());
		if (iter != handlerPatchersMap.end() && !(handlerPatchersMap.key_comp()(indexOption.asIString(), iter->first))) {
			//key found
			continue;
		}

		string optionName = L"Handler";
		optionName += L"_";
		optionName += indexOption.asString();

		const auto descriptionView = rain.readString(optionName);
		if (descriptionView.empty()) {
			log.error(L"Description of '{}' not found", indexOption.asString());
			continue;
		}

		utils::OptionParser optionParser;
		optionParser.setSource(descriptionView);
		auto optionMap = optionParser.parse().asMap(L'|', L' ');

		utils::Rainmeter::ContextLogger cl { rain.getLogger() };
		cl.setPrefix(L"Handler {}: ", indexOption.asString());
		auto patcher = parseHandler(optionMap, cl);
		if (patcher == nullptr) {
			cl.error(L"not a valid description");
			continue;
		}
		const auto unusedOptions = optionMap.getListOfUntouched();
		if (unusedOptionsWarning && !unusedOptions.empty()) {
			cl.warning(L"unused options {}", unusedOptions);
		}

		handlerPatchersMap.insert(iter, decltype(handlerPatchersMap)::value_type(indexOption.asIString(), patcher));
	}
}

std::function<SoundHandler*(SoundHandler*)> ParamParser::parseHandler(const utils::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl) {
	const auto type = optionMap.get(L"type"sv).asIString();

	if (type.empty()) {
		return nullptr;
	}

	// source must be checked to prevent loops
	const auto source = optionMap.getUntouched(L"source").asIString();
	if (!source.empty() && handlerPatchersMap.find(source) == handlerPatchersMap.end()) {
		cl.error(L"reverse or unknown dependency {}", source);
		return nullptr;
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
		return parseHandlerT<FiniteTimeFilter>(optionMap, cl);
	}
	if (type == L"LogarithmicValueMapper") {
		return parseHandlerT<LogarithmicValueMapper>(optionMap, cl);
	}
	if (type == L"spectrogram") {
		return parseHandlerT2<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform") {
		return parseHandlerT2<WaveForm>(optionMap, cl);
	}

	cl.error(L"unknown type '{}'", type);

	return nullptr;
}
