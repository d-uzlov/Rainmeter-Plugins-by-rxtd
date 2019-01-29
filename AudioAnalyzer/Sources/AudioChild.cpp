/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioChild.h"
#include <algorithm>
#include "OptionParser.h"

#include "undef.h"

rxaa::AudioChild::AudioChild(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	const auto parentName = rain.readString(L"Parent") % ciView();
	if (parentName == L"") {
		log.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}
	parent = AudioParent::findInstance(rain.getSkin(), parentName);

	if (parent == nullptr) {
		log.error(L"Parent '{}' not found", parentName);
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}

	if (parent->getState() == utils::MeasureState::BROKEN) {
		log.error(L"Parent '{}' is broken", parentName);
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}
}

rxaa::AudioChild::~AudioChild() { }

void rxaa::AudioChild::_reload() {
	const auto channelStr = rain.readString(L"Channel");

	if (channelStr == L"") {
		channel = Channel::AUTO;
	} else {
		auto channelOpt = Channel::channelParser.find(channelStr);
		if (!channelOpt.has_value()) {
			log.error(L"Invalid Channel '{}', set to Auto.", channelStr);
			channel = Channel::AUTO;
		} else {
			channel = channelOpt.value();
		}
	}

	valueId = rain.readString(L"ValueId");
	correctingConstant = rain.readDouble(L"Gain");
	if (correctingConstant <= 0) {
		correctingConstant = 1.0;
	}
	clamp = rain.readBool(L"Clamp01", true);

	const auto stringValueStr = rain.readString(L"StringValue") % ciView();
	if (stringValueStr == L"" || stringValueStr == L"Number") {
		stringValueType = StringValue::NUMBER;
	} else if (stringValueStr == L"Info") {
		stringValueType = StringValue::INFO;
		const auto infoStr = rain.readString(L"InfoRequest");

		auto requestList = utils::OptionParser {}.asList(infoStr, L',');;

		infoRequest.clear();
		for (auto view : requestList) {
			infoRequest.emplace_back(view);
		}

		infoRequestC.clear();
		for (const auto& str : infoRequest) {
			infoRequestC.push_back(str.c_str());
		}
	} else {
		log.error(L"Invalid StringValue '{}', set to Number.", stringValueStr);
		stringValueType = StringValue::NUMBER;
	}

	// TODO default number transform in handlers

	const auto typeStr = rain.readString(L"NumberTransform") % ciView();
	if (typeStr == L"" || typeStr == L"Linear") {
		numberTransform = NumberTransform::LINEAR;
	} else if (typeStr == L"DB") {
		numberTransform = NumberTransform::DB;
	} else if (typeStr == L"None") {
		numberTransform = NumberTransform::NONE;
	} else {
		log.error(L"Invalid NumberTransform '{}', set to Linear.", typeStr);
		numberTransform = NumberTransform::LINEAR;
	}

	auto signedIndex = rain.readInt(L"Index");
	if (signedIndex < 0) {
		log.error(L"Invalid Index {}. Index should be > 0. Set to 0.", signedIndex);
		signedIndex = 0;
	}
	valueIndex = static_cast<decltype(valueIndex)>(signedIndex);
}

std::tuple<double, const wchar_t*> rxaa::AudioChild::_update() {

	double result;

	switch (numberTransform) {
	case NumberTransform::LINEAR:
	case NumberTransform::DB:
	{
		result = parent->getValue(valueId, channel, valueIndex);
		if (numberTransform == NumberTransform::LINEAR) {
			result = result * correctingConstant;
		} else { // NumberTransform::DB
			result = 20.0 / correctingConstant * std::log10(result) + 1.0;
		}
		break;
	}

	case NumberTransform::NONE:
		result = 0.0;
		break;

	default:
		log.error(L"Unexpected numberTransform: '{}'", numberTransform);
		setMeasureState(utils::MeasureState::BROKEN);
		result = 0.0;
		break;
	}
	if (clamp) {
		result = std::clamp(result, 0.0, 1.0);
	}

	const wchar_t *stringRes = stringValue.c_str();
	switch (stringValueType) {
	case StringValue::NUMBER:
		stringRes = nullptr;
		break;

	case StringValue::INFO:
		stringValue = parent->resolve(index(infoRequestC.size()), infoRequestC.data());
		break;

	default:
		log.error(L"Unexpected stringValueType: '{}'", stringValueType);
		stringRes = nullptr;
		break;
	}

	return std::make_tuple(result, stringRes);
}
