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
	auto processingIndices = optionParser.asList(rain.readString(L"Processing"), L'|');
	for (auto processingName : processingIndices) {
		utils::Rainmeter::ContextLogger cl { rain.getLogger() };
		cl.setPrefix(L"Processing {}:", processingName);

		string processingOptionIndex = L"Processing"s;
		processingOptionIndex += L"_";
		processingOptionIndex += processingName;
		auto processingMap = optionParser.asMap(rain.readString(processingOptionIndex), L'|', L' ');
		auto channelsString = processingMap.get(L"channels"sv).asString();
		if (channelsString.empty()) {
			cl.error(L"channels not found");
			continue;
		}
		auto handlersString = processingMap.get(L"handlers"sv).asString();
		if (handlersString.empty()) {
			cl.error(L"handlers not found");
			continue;
		}

		auto channels = parseChannels(optionParser.asList(channelsString, L','));
		if (channels.empty()) {
			cl.error(L"no valid channels found");
			continue;
		}
		auto handlersList = optionParser.asList(handlersString, L',');

		cacheHandlers(handlersList);

		for (auto channel : channels) {
			for (auto handlerName : handlersList.viewCI()) {
				handlers[channel].emplace_back(handlerName);
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

std::set<Channel> ParamParser::parseChannels(utils::OptionParser::OptionList channelsStringList) const {
	std::set<Channel> set;

	for (auto channel : channelsStringList) {
		auto opt = Channel::channelParser.find(channel);
		if (!opt.has_value()) {
			log.error(L"Can't parse '{}' as channel", channel);
			continue;
		}
		set.insert(opt.value());
	}

	return set;
}

void ParamParser::cacheHandlers(const utils::OptionParser::OptionList& indices) {
	utils::OptionParser optionParser;

	for (auto index : indices.viewCI()) {

		auto iter = handlerPatchersMap.lower_bound(index);
		if (iter != handlerPatchersMap.end() && !(handlerPatchersMap.key_comp()(index, iter->first))) {
			//key found
			continue;
		}

		istring optionName = L"Handler";
		optionName += L"_";
		optionName += index;

		const auto descriptionView = rain.readString(optionName % csView());
		if (descriptionView.empty()) {
			log.error(L"Description of '{}' not found", index);
			continue;
		}

		auto optionMap = optionParser.asMap(descriptionView, L'|', L' ');

		utils::Rainmeter::ContextLogger cl { rain.getLogger() };
		cl.setPrefix(L"Handler {}: ", index);
		auto patcher = parseHandler(optionMap, cl);
		if (patcher == nullptr) {
			cl.error(L"not a valid description", index);
			continue;
		}
		const auto unusedOptions = optionMap.getListOfUntouched();
		if (unusedOptionsWarning && !unusedOptions.empty()) {
			cl.warning(L"unused options {}", unusedOptions);
		}

		handlerPatchersMap.insert(iter, decltype(handlerPatchersMap)::value_type(index, patcher));
	}
}

std::function<SoundHandler*(SoundHandler*)> ParamParser::parseHandler(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl) {
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
