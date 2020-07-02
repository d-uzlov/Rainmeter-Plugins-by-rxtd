/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioChild.h"
#include "option-parser/OptionList.h"

#include "undef.h"

using namespace audio_analyzer;

AudioChild::AudioChild(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	const auto parentName = rain.readString(L"Parent") % ciView();
	if (parentName == L"") {
		logger.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
	parent = AudioParent::findInstance(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent '{}' is not found or broken", parentName);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

void AudioChild::_reload() {
	const auto channelStr = rain.readString(L"Channel");

	if (channelStr == L"") {
		channel = Channel::eAUTO;
	} else {
		auto channelOpt = Channel::channelParser.find(channelStr);
		if (!channelOpt.has_value()) {
			logger.error(L"Invalid Channel '{}', set to Auto.", channelStr);
			channel = Channel::eAUTO;
		} else {
			channel = channelOpt.value();
		}
	}

	valueId = rain.readString(L"ValueId");

	const auto stringValueStr = rain.readString(L"StringValue") % ciView();
	if (stringValueStr == L"" || stringValueStr == L"Number") {
		setUseResultString(false);
	} else if (stringValueStr == L"Info") {
		setUseResultString(true);

		auto requestList = rain.read(L"InfoRequest").asList(L',').own();

		infoRequest.clear();
		for (auto view : requestList) {
			infoRequest.emplace_back(view.asString());
		}

		infoRequestC.clear();
		for (const auto& str : infoRequest) {
			infoRequestC.push_back(str.c_str());
		}
	} else {
		logger.error(L"Invalid StringValue '{}', set to Number.", stringValueStr);
		setUseResultString(false);
	}

	auto signedIndex = rain.read(L"Index").asInt();
	if (signedIndex < 0) {
		logger.error(L"Invalid Index {}. Index should be > 0. Set to 0.", signedIndex);
		signedIndex = 0;
	}
	valueIndex = static_cast<decltype(valueIndex)>(signedIndex);
}

double AudioChild::_update() {
	return parent->getValue(valueId, channel, valueIndex);
}

void AudioChild::_updateString(string& resultStringBuffer) {
	resultStringBuffer = parent->resolve(index(infoRequestC.size()), infoRequestC.data());
}
