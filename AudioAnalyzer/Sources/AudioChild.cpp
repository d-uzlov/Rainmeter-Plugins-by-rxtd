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

using namespace audio_analyzer;

AudioChild::AudioChild(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	const auto parentName = rain.read(L"Parent").asIString();
	if (parentName.empty()) {
		logger.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
	parent = utils::ParentBase::find<AudioParent>(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent '{}' doesn't exist or is broken", parentName);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

void AudioChild::vReload() {
	if (parent->getState() != utils::MeasureState::eWORKING) {
		return;
	}

	const auto channelStr = rain.read(L"Channel").asIString(L"auto");
	auto channelOpt = Channel::channelParser.find(channelStr);
	if (!channelOpt.has_value()) {
		logger.error(L"Invalid Channel '{}', set to Auto.", channelStr);
		channel = Channel::eAUTO;
	} else {
		channel = channelOpt.value();
	}

	handlerName = rain.read(L"ValueId").asIString();
	if (handlerName.empty()) {
		logger.error(L"ValueID can't be empty");
		setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		return;
	}

	procName = rain.read(L"Processing").asIString();

	const auto stringValueStr = rain.read(L"StringValue").asIString(L"Number");
	if (stringValueStr == L"Number") {
		setUseResultString(false);
	} else if (stringValueStr == L"Info") {
		setUseResultString(true);

		auto requestList = rain.read(L"InfoRequest").asList(L',');

		infoRequest.clear();
		for (auto view : requestList) {
			infoRequest.emplace_back(view.asIString());
		}

		infoRequestC.clear();
		for (const auto& str : infoRequest) {
			infoRequestC.push_back(str);
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

	const index legacyNumber = parent->getLegacyNumber();
	if (legacyNumber < 104) {
		legacy_readOptions();
		legacy.use = true;
	} else {
		legacy.use = false;

		const auto error = parent->checkHandler(procName, channel, handlerName);
		if (!error.empty()) {
			logger.error(L"Invalid options: {}", error);
			setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		}
	}
}

double AudioChild::vUpdate() {
	switch (parent->getState()) {
	case utils::MeasureState::eWORKING: break;
	case utils::MeasureState::eTEMP_BROKEN: return 0.0;
	case utils::MeasureState::eBROKEN:
		logger.error(L"stopping updating because parent measure is broken");
		setMeasureState(utils::MeasureState::eBROKEN);
		break;
	}

	if (legacy.use) {
		return legacy_update();
	}

	return parent->getValue(procName, handlerName, channel, valueIndex);
}

void AudioChild::vUpdateString(string& resultStringBuffer) {
	resultStringBuffer = parent->resolve(infoRequestC);
}

void AudioChild::legacy_readOptions() {
	if (const auto numTr = rain.read(L"NumberTransform").asIString(L"Linear");
		numTr == L"Linear") {
		legacy.numberTransform = Legacy::NumberTransform::eLINEAR;
	} else if (numTr == L"DB") {
		legacy.numberTransform = Legacy::NumberTransform::eDB;
	} else if (numTr == L"None") {
		legacy.numberTransform = Legacy::NumberTransform::eNONE;
	} else {
		logger.error(L"Invalid NumberTransform '{}', set to Linear.", numTr);
		legacy.numberTransform = Legacy::NumberTransform::eLINEAR;
	}
	legacy.clamp01 = rain.read(L"Clamp01").asBool(true);

	legacy.correctingConstant = rain.read(L"Gain").asFloat();
	if (legacy.correctingConstant <= 0) {
		legacy.correctingConstant = 1.0;
	}
}

double AudioChild::legacy_update() {
	double result = 0.0;

	switch (legacy.numberTransform) {
	case Legacy::NumberTransform::eLINEAR:
		result = parent->legacy_getValue(handlerName, channel, valueIndex);
		result = result * legacy.correctingConstant;
		break;

	case Legacy::NumberTransform::eDB:
		result = parent->legacy_getValue(handlerName, channel, valueIndex);
		result = 20.0 / legacy.correctingConstant * std::log10(result) + 1.0;
		break;

	case Legacy::NumberTransform::eNONE:
	default:
		break;
	}
	if (legacy.clamp01) {
		result = std::clamp(result, 0.0, 1.0);
	}

	return result;
}
