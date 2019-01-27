/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ParamParser.h"
#include "sound-handlers/BlockMean.h"
#include "sound-handlers/FftAnalyzer.h"
#include "sound-handlers/BandAnalyzer.h"

#include "sound-handlers/Spectrogram.h"
#include "sound-handlers/WaveForm.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

rxaa::ParamParser::ParamParser(utils::Rainmeter& rain) : rain(rain), log(rain.getLogger()) { }

void rxaa::ParamParser::parse() {
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
			for (auto handlerName : handlersList) {
				handlers[channel].emplace_back(handlerName);
			}
		}
	}
}

const std::map<rxaa::Channel, std::vector<string>>& rxaa::ParamParser::getHandlers() const {
	return handlers;
}

const std::map<string, std::function<rxaa::SoundHandler*(rxaa::SoundHandler*)>, std::less<>>&
rxaa::ParamParser::getPatches() const {
	return handlerPatchersMap;
}

std::set<rxaa::Channel> rxaa::ParamParser::parseChannels(utils::OptionParser::OptionList channelsStringList) const {
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

void rxaa::ParamParser::cacheHandlers(utils::OptionParser::OptionList indices) {
	utils::OptionParser optionParser;

	for (auto index : indices) {

		auto iter = handlerPatchersMap.lower_bound(index);
		if (iter != handlerPatchersMap.end() && !(handlerPatchersMap.key_comp()(index, iter->first))) {
			//key found
			continue;
		}

		string optionName = L"Handler"s;
		optionName += L"_";
		optionName += index;
		auto optionMap = optionParser.asMap(rain.readString(optionName), L'|', L' ');

		utils::Rainmeter::ContextLogger cl { rain.getLogger() };
		cl.setPrefix(L"Handler {}:", index);
		auto patcher = parseHandler(optionMap, cl);
		if (patcher == nullptr) {
			log.error(L"Handler {}: not a valid description", index);
			continue;
		}

		handlerPatchersMap.insert(iter, decltype(handlerPatchersMap)::value_type(index, patcher));
	}
}

std::function<rxaa::SoundHandler*(rxaa::SoundHandler*)> rxaa::ParamParser::parseHandler(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl) {
	const auto type = optionMap.get(L"type"sv).asString();

	if (type.empty()) {
		return nullptr;
	}

	if (type == L"rms"sv) {
		return parseHandlerT<BlockRms>(optionMap, cl);
	}
	if (type == L"peak"sv) {
		return parseHandlerT<BlockPeak>(optionMap, cl);
	}
	if (type == L"fft"sv) {
		return parseHandlerT<FftAnalyzer>(optionMap, cl);
	}
	if (type == L"band"sv) {
		return parseHandlerT2<BandAnalyzer>(optionMap, cl);
	}
	if (type == L"spectrogram"sv) {
		return parseHandlerT2<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform"sv) {
		return parseHandlerT2<WaveForm>(optionMap, cl);
	}

	return nullptr;
}
