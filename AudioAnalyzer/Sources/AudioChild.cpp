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

	auto newOptions = readOptions();
	if (newOptions == options) {
		return;
	}

	options = std::move(newOptions);
	setUseResultString(!options.infoRequest.empty());

	if (!options.procName.empty()) {
		const auto success = parent->isHandlerShouldExist(options.procName, options.channel, options.handlerName);
		if (!success) {
			logger.error(L"Measure is invalid, see parent measure log message for reason");
			setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		}
	}
}

double AudioChild::vUpdate() {
	if (options.handlerName.empty()) {
		return 0.0;
	}

	double result;
	if (legacyNumber < 104) {
		result = legacy_update();
	} else {
		result = parent->getValue(options.procName, options.handlerName, options.channel, options.valueIndex);
	}

	result = options.transformer.apply(result);

	return result;
}

void AudioChild::vUpdateString(string& resultStringBuffer) {
	resultStringBuffer = parent->resolve(options.infoRequestC);
}

AudioChild::Options AudioChild::readOptions() const {
	Options result{};
	const auto channelStr = rain.read(L"Channel").asIString(L"auto");
	auto channelOpt = ChannelUtils::parse(channelStr);
	if (!channelOpt.has_value()) {
		logger.error(L"Invalid Channel '{}', set to Auto.", channelStr);
		result.channel = Channel::eAUTO;
	} else {
		result.channel = channelOpt.value();
	}

	result.handlerName = rain.read(L"handler").asIString();
	if (result.handlerName.empty()) {
		result.handlerName = rain.read(L"ValueId").asIString();
	}

	if (!result.handlerName.empty()) {
		result.procName = rain.read(L"Processing").asIString();
		if (result.procName.empty()) {
			result.procName = parent->findProcessingFor(result.handlerName);
		}

		result.valueIndex = rain.read(L"Index").asInt();
		if (result.valueIndex < 0) {
			logger.error(L"Invalid Index {}. Index should be > 0. Set to 0.", result.valueIndex);
			result.valueIndex = 0;
		}

		auto transformDesc = rain.read(L"Transform");
		auto transformLogger = logger.context(L"Transform: ");
		result.transformer = CVT::parse(transformDesc.asString(), transformLogger);
	}

	if (const auto stringValueStr = rain.read(L"StringValue").asIString(L"Number");
		stringValueStr == L"Info") {
		auto requestList = rain.read(L"InfoRequest").asList(L',');

		result.infoRequest.clear();
		for (auto view : requestList) {
			result.infoRequest.emplace_back(view.asIString());
		}

		result.infoRequestC.clear();
		for (const auto& str : result.infoRequest) {
			result.infoRequestC.push_back(str);
		}
	} else if (stringValueStr == L"Number") {
		// no need to do anything here
	} else {
		logger.error(L"Invalid StringValue '{}', set to Number.", stringValueStr);
	}

	if (legacyNumber < 104) {
		result.legacy = legacy_readOptions();
	}

	return result;
}

AudioChild::Options::Legacy AudioChild::legacy_readOptions() const {
	Options::Legacy legacy{};

	if (const auto numTr = rain.read(L"NumberTransform").asIString(L"Linear");
		numTr == L"Linear") {
		legacy.numberTransform = Options::Legacy::NumberTransform::eLINEAR;
	} else if (numTr == L"DB") {
		legacy.numberTransform = Options::Legacy::NumberTransform::eDB;
	} else if (numTr == L"None") {
		legacy.numberTransform = Options::Legacy::NumberTransform::eNONE;
	} else {
		logger.error(L"Invalid NumberTransform '{}', set to Linear.", numTr);
		legacy.numberTransform = Options::Legacy::NumberTransform::eLINEAR;
	}
	legacy.clamp01 = rain.read(L"Clamp01").asBool(true);

	legacy.correctingConstant = rain.read(L"Gain").asFloat();
	if (legacy.correctingConstant <= 0) {
		legacy.correctingConstant = 1.0;
	}

	return legacy;
}

double AudioChild::legacy_update() const {
	double result = 0.0;

	switch (options.legacy.numberTransform) {
	case Options::Legacy::NumberTransform::eLINEAR:
		result = parent->getValue(options.procName, options.handlerName, options.channel, options.valueIndex);
		result = result * options.legacy.correctingConstant;
		break;

	case Options::Legacy::NumberTransform::eDB:
		result = parent->getValue(options.procName, options.handlerName, options.channel, options.valueIndex);
		result = 20.0 / options.legacy.correctingConstant * std::log10(result) + 1.0;
		break;

	case Options::Legacy::NumberTransform::eNONE:
		break;
	}
	if (options.legacy.clamp01) {
		result = std::clamp(result, 0.0, 1.0);
	}

	return result;
}
