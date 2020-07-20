/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"

#include "undef.h"
#include <numeric>
#include "LinearInterpolator.h"
#include "../../audio-utils/LogarithmicIRF.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionSequence.h"
#include "option-parser/OptionList.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BlockHandler::Params> BlockHandler::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
	Params params;

	params.resolution = optionMap.get(L"resolution"sv).asFloat(1000.0 / 60.0);
	if (params.resolution <= 0) {
		cl.warning(L"block must be > 0 but {} found. Assume 10", params.resolution);
		params.resolution = 10;
	}
	params.resolution *= 0.001;

	params.subtractMean = optionMap.get(L"subtractMean").asBool(true);

	auto transform = optionMap.get(L"transform");
	if (!transform.empty()) {
		auto transformSequence = transform.asSequence();
		for (auto list : transformSequence) {
			const auto transformName = list.get(0).asIString();
			auto transformOpt = parseTransformation(list, cl);
			if (!transformOpt.has_value()) {
				cl.error(L"transform '{}' is not recognized, using default transform sequence", transformName);
				params.transformations = { };
				break;
			}
			params.transformations.emplace_back(transformOpt.value());
		}
	}

	return params;
}

void BlockHandler::setNextValue(double value) {
	for (const auto& transform : params.transformations) {
		switch (transform.type) {
		case TransformType::eFILTER: {
			auto& filter = *static_cast<audio_utils::LogarithmicIRF*>(transform.state);
			value = filter.next(value);
			break;
		}
		case TransformType::eDB: {
			value = std::max<double>(value, std::numeric_limits<float>::min());
			value = 10.0 * std::log10(value);
			break;
		}
		case TransformType::eMAP: {
			auto& interpolator = *static_cast<utils::LinearInterpolator*>(transform.state);
			value = interpolator.toValue(value);
			break;
		}
		case TransformType::eCLAMP: {
			value = std::clamp(value, transform.args[0], transform.args[1]);
			break;
		}
		default: std::terminate();
		}
	}
	result = value;
}

BlockHandler::Transformation::~Transformation() {
	if (state != nullptr) {
		switch (type) {
		case TransformType::eFILTER:
			delete static_cast<audio_utils::LogarithmicIRF*>(state);
			break;
		case TransformType::eDB: break;
		case TransformType::eMAP:
			delete static_cast<utils::LinearInterpolator*>(state);
			break;
		case TransformType::eCLAMP: break;
		default: ;
		}
	}
}

void BlockHandler::setParams(Params params, Channel channel) {
	if (this->params == params) {
		return;
	}

	this->params = params;

	recalculateConstants();

	if (this->params.transformations.empty()) {
		auto transformSequence = utils::Option{ getDefaultTransform() }.asSequence();
		for (auto list : transformSequence) {
			utils::Rainmeter::Logger dummyLogger;
			auto transformOpt = parseTransformation(list, dummyLogger);
			this->params.transformations.emplace_back(transformOpt.value());
		}
	}
}

void BlockHandler::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	recalculateConstants();

	_setSamplesPerSec(samplesPerSec);
}

const wchar_t* BlockHandler::getProp(const isview& prop) const {
	if (prop == L"block size") {
		propString = std::to_wstring(blockSize);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void BlockHandler::reset() {
	counter = 0;
	result = 0.0;
	resetTransformationStates();
	_reset();
}

void BlockHandler::process(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();

	float mean = 0.0;
	if (isAverageNeeded()) {
		mean = std::accumulate(wave.begin(), wave.end(), 0.0f) / wave.size();
	}

	_process(wave, mean);
}

std::optional<BlockHandler::Transformation> BlockHandler::parseTransformation(
	utils::OptionList list, utils::Rainmeter::Logger& cl) {
	const auto transformName = list.get(0).asIString();
	Transformation tr{ };
	index paramCount;
	if (transformName == L"filter") {
		tr.type = TransformType::eFILTER;
		paramCount = 2;
	} else if (transformName == L"db") {
		tr.type = TransformType::eDB;
		paramCount = 0;
	} else if (transformName == L"map") {
		tr.type = TransformType::eMAP;
		paramCount = 4;
	} else if (transformName == L"clamp") {
		tr.type = TransformType::eCLAMP;
		paramCount = 2;
	} else {
		return std::nullopt;
	}

	if (list.size() != paramCount + 1) {
		cl.error(L"wrong params count for {}: {} instead of {}", transformName, list.size() - 1, paramCount);
		return std::nullopt;
	}

	for (int i = 1; i < list.size(); ++i) {
		tr.args[i - 1] = list.get(i).asFloat();
	}
	return tr;
}

void BlockHandler::recalculateConstants() {
	auto test = samplesPerSec * params.resolution;
	blockSize = static_cast<decltype(blockSize)>(test);
	if (blockSize < 1) {
		blockSize = 1;
	}

	updateTransformations();
}

void BlockHandler::updateTransformations() {
	for (const auto& transform : params.transformations) {
		switch (transform.type) {
		case TransformType::eFILTER: {
			if (transform.state == nullptr) {
				transform.state = new audio_utils::LogarithmicIRF{ };
			}
			auto& filter = *static_cast<audio_utils::LogarithmicIRF*>(transform.state);
			filter.setParams(transform.args[0] * 0.001, transform.args[1] * 0.001, samplesPerSec, blockSize);
			break;
		}
		case TransformType::eDB: break;
		case TransformType::eMAP: {
			if (transform.state == nullptr) {
				transform.state = new utils::LinearInterpolator{ };
			}
			auto& interpolator = *static_cast<utils::LinearInterpolator*>(transform.state);

			if (std::abs(transform.args[0] - transform.args[1]) < std::numeric_limits<float>::min()) {
				interpolator.setParams(0.0, 1.0, transform.args[2], transform.args[3]);
			} else {
				interpolator.setParams(transform.args[0], transform.args[1], transform.args[2], transform.args[3]);
			}

			break;
		}
		case TransformType::eCLAMP: break;
		default: std::terminate();
		}
	}
}

void BlockHandler::resetTransformationStates() {
	for (const auto& transform : params.transformations) {
		if (transform.type == TransformType::eFILTER) {
			auto filter = static_cast<audio_utils::LogarithmicIRF*>(transform.state);
			if (filter != nullptr) {
				filter->reset();
			}
		}
	}
}

void BlockHandler::processSilence(const DataSupplier& dataSupplier) {
	const auto waveSize = dataSupplier.getWave().size();
	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const index missingPoints = blockSize - counter;
		if (waveProcessed + missingPoints <= waveSize) {
			finishBlock();
			waveProcessed += missingPoints;
		} else {
			const auto waveRemainder = waveSize - waveProcessed;
			counter = waveRemainder;
			break;
		}
	}
}

void BlockRms::_process(array_view<float> wave, float average) {
	for (double x : wave) {
		x -= average;
		intermediateResult += x * x;
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void BlockRms::finishBlock() {
	const double value = std::sqrt(intermediateResult / getBlockSize());
	setNextValue(value);
	counter = 0;
	intermediateResult = 0.0;
}

void BlockRms::_reset() {
	intermediateResult = 0.0;
}

sview BlockRms::getDefaultTransform() {
	return L"db map[-70, 0][0, 1] clamp[0, 1] filter[200, 200]"sv;
}

void BlockPeak::_process(array_view<float> wave, float average) {
	for (double x : wave) {
		x -= average;
		intermediateResult = std::max<double>(intermediateResult, std::abs(x));
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void BlockPeak::finishBlock() {
	setNextValue(intermediateResult);
	counter = 0;
	intermediateResult = 0.0;
}

void BlockPeak::_reset() {
	intermediateResult = 0.0;
}

sview BlockPeak::getDefaultTransform() {
	return L"filter[0, 500]"sv;
}
