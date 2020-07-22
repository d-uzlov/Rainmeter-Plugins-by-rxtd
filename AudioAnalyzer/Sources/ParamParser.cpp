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

void ParamParser::parse() {
	handlerPatchersMap.clear();

	auto processingIndices = rain.read(L"Processing").asList(L'|');
	if (!checkListUnique(processingIndices)) {
		rain.getLogger().error(L"Found repeating processings, aborting");
		return;
	}

	for (const auto& processingNameOption : processingIndices) {
		auto processingName = processingNameOption.asString();
		auto cl = rain.getLogger().context(L"Processing {}: ", processingName);

		string processingOptionIndex = L"Processing-"s;
		processingOptionIndex += processingName;
		auto processingDescriptionOption = rain.read(processingOptionIndex);
		if (processingDescriptionOption.empty()) {
			processingOptionIndex = L"Processing_"s;
			processingOptionIndex += processingName;
			processingDescriptionOption = rain.read(processingOptionIndex);
		}

		if (processingDescriptionOption.empty()) {
			cl.error(L"processing description not found");
			continue;
		}

		auto processingMap = processingDescriptionOption.asMap(L'|', L' ');
		auto channelsList = processingMap.get(L"channels"sv).asList(L',');
		if (channelsList.empty()) {
			cl.error(L"channels not found");
			continue;
		}
		auto channels = parseChannels(channelsList);
		if (channels.empty()) {
			cl.error(L"no valid channels found");
			continue;
		}

		auto handlersOption = processingMap.get(L"handlers"sv);
		if (handlersOption.empty()) {
			cl.error(L"handlers not found");
			continue;
		}

		auto handlersList = handlersOption.asList(L',');
		if (!checkListUnique(handlersList)) {
			cl.error(L"found repeating handlers, aborting");
			return;
		}

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

const std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>>&
ParamParser::getPatches() const {
	return handlerPatchersMap;
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

void ParamParser::cacheHandlers(const utils::OptionList& indices) {
	for (auto indexOption : indices) {

		auto iter = handlerPatchersMap.lower_bound(indexOption.asIString());
		if (iter != handlerPatchersMap.end()
			&& !(handlerPatchersMap.key_comp()(indexOption.asIString(), iter->first))
		) {
			//key found
			continue;
		}

		string optionName = L"Handler-";
		optionName += indexOption.asString();

		auto descriptionOption = rain.read(optionName);

		if (descriptionOption.empty()) {
			optionName = L"Handler_";
			optionName += indexOption.asString();
			descriptionOption = rain.read(optionName);
		}

		if (descriptionOption.empty()) {
			log.error(L"Description of '{}' not found", indexOption.asString());
			continue;
		}

		auto optionMap = descriptionOption.asMap(L'|', L' ');

		auto cl = rain.getLogger().context(L"Handler '{}': ", indexOption.asString());
		auto patcher = parseHandler(optionMap, cl);
		if (patcher == nullptr) {
			cl.error(L"not a valid description");
			continue;
		}
		const auto unusedOptions = optionMap.getListOfUntouched();
		if (unusedOptionsWarning && !unusedOptions.empty()) {
			cl.warning(L"unused option: '{}'", unusedOptions);
		}

		handlerPatchersMap.insert(iter, decltype(handlerPatchersMap)::value_type(indexOption.asIString(), patcher));
	}
}

std::function<SoundHandler*(SoundHandler*, Channel)> ParamParser::parseHandler(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
	const auto type = optionMap.get(L"type"sv).asIString();

	if (type.empty()) {
		return nullptr;
	}

	// source must be checked to prevent loops
	const auto source = optionMap.getUntouched(L"source").asIString();
	if (!source.empty() && handlerPatchersMap.find(source) == handlerPatchersMap.end()) {
		cl.error(L"reverse or unknown dependency '{}'", source);
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
