/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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
		logger.error(L"Parent '{}' doesn't exist", parentName);
		throw std::runtime_error{ "" };
	}

	if (!parent->isValid()) {
		throw std::runtime_error{ "" };
	}

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

	return result;
}
