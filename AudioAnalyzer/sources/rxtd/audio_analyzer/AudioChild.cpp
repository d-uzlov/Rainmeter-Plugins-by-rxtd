// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "AudioChild.h"
#include "rxtd/option_parsing/OptionList.h"

using rxtd::audio_analyzer::AudioChild;

AudioChild::AudioChild(Rainmeter&& _rain) : MeasureBase(std::move(_rain)) {
	const auto parentName = rain.read(L"Parent").asIString();
	if (parentName.empty()) {
		logger.error(L"Parent must be specified");
		throw std::runtime_error{ "" };
	}
	parent = utils::ParentMeasureBase::find<AudioParent>(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent doesn't exist: {}", parentName);
		throw std::runtime_error{ "" };
	}

	if (!parent->isValid()) {
		throw std::runtime_error{ "" };
	}

	parser.setLogger(logger);

	version = parent->getVersion();
}

void AudioChild::vReload() {
	auto newOptions = readOptions();
	if (newOptions == options) {
		return;
	}

	options = std::move(newOptions);
	setUseResultString(!options.infoRequest.empty());

	if (!options.procName.empty()) {
		const auto success = parent->checkHandlerShouldExist(options.procName, options.channel, options.handlerName);
		if (!success) {
			logger.error(L"Measure is invalid, see parent measure log message for reason");
			setInvalid();
		}
	}
}

double AudioChild::vUpdate() {
	if (options.handlerName.empty()) {
		return 0.0;
	}

	double result = parent->getValue(options.procName, options.handlerName, options.channel, options.valueIndex);

	result = options.transformer.apply(result);

	return result;
}

void AudioChild::vUpdateString(string& resultStringBuffer) {
	resultStringBuffer = parent->resolve(options.infoRequestC);
}

AudioChild::Options AudioChild::readOptions() {
	Options result{};
	const auto channelStr = rain.read(L"Channel").asIString(L"auto");
	auto channelOpt = ChannelUtils::parse(channelStr);
	if (!channelOpt.has_value()) {
		logger.error(L"Invalid Channel: {}", channelStr);
		setInvalid();
		return options;
	}
	result.channel = channelOpt.value();

	result.handlerName = rain.read(L"handlerName").asIString();

	if (!result.handlerName.empty()) {
		result.procName = rain.read(L"unit").asIString();
		if (result.procName.empty()) {
			result.procName = parent->findProcessingFor(result.handlerName);
		}

		result.valueIndex = parser.parse(rain.read(L"Index"), L"Index").valueOr(0);
		if (result.valueIndex < 0) {
			logger.error(L"Invalid Index {}. Index should be >= 0. Set to 0.", result.valueIndex);
			result.valueIndex = 0;
		}

		auto transformDesc = rain.read(L"Transform");
		result.transformer = CVT::parse(transformDesc.asString(), parser, logger.context(L"Transform: "));
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
		logger.error(L"Invalid StringValue: {}", stringValueStr);
		setInvalid();
		return options;
	}

	return result;
}
