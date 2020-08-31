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
		logger.error(L"Parent '{}' doesn't exist", parentName);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	if (parent->getState() == utils::MeasureState::eBROKEN) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	legacyNumber = parent->getLegacyNumber();
	if (!(legacyNumber < 104)) {
		auto type = rain.read(L"Type").asIString();
		if (type != L"Child") {
			rain.createLogger().warning(L"Unknown type '{}', defaulting to Child is deprecated", type);
		}
	}
}

void AudioChild::vReload() {
	if (parent->getState() == utils::MeasureState::eTEMP_BROKEN) {
		setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		return;
	}

	const auto channelStr = rain.read(L"Channel").asIString(L"auto");
	auto channelOpt = ChannelUtils::parse(channelStr);
	if (!channelOpt.has_value()) {
		logger.error(L"Invalid Channel '{}', set to Auto.", channelStr);
		channel = Channel::eAUTO;
	} else {
		channel = channelOpt.value();
	}

	handlerName = rain.read(L"ValueId").asIString();

	procName = rain.read(L"Processing").asIString();

	valueIndex = rain.read(L"Index").asInt();
	if (valueIndex < 0) {
		logger.error(L"Invalid Index {}. Index should be > 0. Set to 0.", valueIndex);
		valueIndex = 0;
	}

	auto transformDesc = rain.read(L"Transform");
	auto transformLogger = logger.context(L"Transform: ");
	transformer = audio_utils::TransformationParser::parse(transformDesc.asString(), transformLogger);

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

	if (!handlerName.empty()) {
		if (legacyNumber < 104) {
			legacy_readOptions();
			procName = parent->legacy_findProcessingFor(handlerName);
		}

		const auto error = parent->checkHandler(procName, channel, handlerName);
		if (!error.empty()) {
			logger.error(L"Invalid options: {}", error);
			handlerName = { };
		}
	}
}

double AudioChild::vUpdate() {
	if (handlerName.empty()) {
		return 0.0;
	}

	double result;
	if (legacyNumber < 104) {
		result = legacy_update();
	} else {
		result = parent->getValue(procName, handlerName, channel, valueIndex);
	}

	// transformer.setParams(); // todo
	result = transformer.apply(result);

	return result;
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
		result = parent->getValue(procName, handlerName, channel, valueIndex);
		result = result * legacy.correctingConstant;
		break;

	case Legacy::NumberTransform::eDB:
		result = parent->getValue(procName, handlerName, channel, valueIndex);
		result = 20.0 / legacy.correctingConstant * std::log10(result) + 1.0;
		break;

	case Legacy::NumberTransform::eNONE:
		break;
	}
	if (legacy.clamp01) {
		result = std::clamp(result, 0.0, 1.0);
	}

	return result;
}
