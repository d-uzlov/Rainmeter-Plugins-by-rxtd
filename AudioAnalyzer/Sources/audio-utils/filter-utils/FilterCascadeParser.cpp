/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FilterCascadeParser.h"
#include "InfiniteResponseFilter.h"
#include "BiQuadIIR.h"
#include "BQFilterBuilder.h"
#include "RainmeterWrappers.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionSequence.h"
#include "../butterworth-lib/ButterworthWrapper.h"

using namespace audio_utils;

FilterCascade FilterCascadeCreator::getInstance(double samplingFrequency) const {
	std::vector<std::unique_ptr<AbstractFilter>> result;
	for (const auto& patcher : patchers) {
		result.push_back(patcher(samplingFrequency));
	}

	FilterCascade fc{ std::move(result) };
	return fc;
}

FilterCascadeCreator FilterCascadeParser::parse(const utils::Option& description, utils::Rainmeter::Logger& logger) {
	std::vector<FCF> result;

	for (auto filterDescription : description.asSequence()) {
		auto patcher = parseFilter(filterDescription, logger);
		if (patcher == nullptr) {
			return { };
		}

		result.push_back(std::move(patcher));
	}

	return FilterCascadeCreator{ description.asString() % own(), result };
}

FilterCascadeParser::FCF
FilterCascadeParser::parseFilter(const utils::OptionList& description, utils::Rainmeter::Logger& logger) {
	auto name = description.get(0).asIString();

	if (name.empty()) {
		logger.error(L"filter name is not found", name);
		return { };
	}

	auto cl = logger.context(L"'{}': ", name);
	const auto args = description.get(1).asMap(L',', L' ');

	if (utils::StringUtils::checkStartsWith(name, { L"bq" })) {
		return parseBQ(name, args, cl);
	}

	if (utils::StringUtils::checkStartsWith(name, { L"bw" })) {
		return parseBW(name, args, cl);
	}

	cl.error(L"unknown filter type");
	return { };
}

FilterCascadeParser::FCF
FilterCascadeParser::parseBQ(isview name, const utils::OptionMap& description, utils::Rainmeter::Logger& cl) {
	if (!description.has(L"q")) {
		cl.error(L"Q is not found", name);
		return { };
	}
	if (!description.has(L"freq")) {
		cl.error(L"freq is not found", name);
		return { };
	}

	const double q = std::max<double>(description.get(L"q").asFloat(), std::numeric_limits<float>::epsilon());
	const double centralFrequency = std::max<double>(
		description.get(L"freq").asFloat(),
		std::numeric_limits<float>::epsilon()
	);
	double gain = description.get(L"gain").asFloat();

	using FilterCreationFunc = BiQuadIIR (*)(double samplingFrequency, double Q, double freq, double dbGain);
	FilterCreationFunc filterCreationFunc;

	if (name == L"bqHighPass") {
		filterCreationFunc = BQFilterBuilder::createHighPass;
		gain = 0.0;
	} else if (name == L"bqLowPass") {
		filterCreationFunc = BQFilterBuilder::createLowPass;
		gain = 0.0;
	} else if (name == L"bqHighShelf") {
		if (!description.has(L"gain")) {
			cl.error(L"gain is not found", name);
			return { };
		}

		filterCreationFunc = BQFilterBuilder::createHighShelf;
	} else if (name == L"bqLowShelf") {
		if (!description.has(L"gain")) {
			cl.error(L"gain is not found", name);
			return { };
		}

		filterCreationFunc = BQFilterBuilder::createLowShelf;
	} else if (name == L"bqPeak") {
		if (!description.has(L"gain")) {
			cl.error(L"gain is not found", name);
			return { };
		}

		filterCreationFunc = BQFilterBuilder::createPeak;
	} else {
		cl.error(L"unknown filter type");
		return { };
	}

	return [=](double sampleFrequency) {
		auto filter = new BiQuadIIR{ };
		*filter = filterCreationFunc(sampleFrequency, q, centralFrequency, gain);
		if (gain > 0.0) {
			filter->addGain(-gain);
		}
		return std::unique_ptr<AbstractFilter>{ filter };
	};
}

FilterCascadeParser::FCF
FilterCascadeParser::parseBW(isview name, const utils::OptionMap& description, utils::Rainmeter::Logger& cl) {
	if (!description.has(L"order")) {
		cl.error(L"order is not found");
		return { };
	}

	const index order = description.get(L"order").asInt();
	if (order <= 0 || order > 15) {
		cl.error(L"order must be in range [1, 15] but {} found", order);
		return { };
	}

	const double cutoff = std::max<double>(description.get(L"freq").asFloat(), std::numeric_limits<float>::epsilon());
	const double cutoffLow = std::max<double>(
		description.get(L"freqLow").asFloat(),
		std::numeric_limits<float>::epsilon()
	);
	const double cutoffHigh = std::max<double>(
		description.get(L"freqHigh").asFloat(),
		std::numeric_limits<float>::epsilon()
	);

	if (name == L"bwLowPass" || name == L"bwHighPass") {
		if (!description.has(L"freq")) {
			cl.error(L"freq is not found");
			return { };
		}

		if (name == L"bwLowPass") {
			return createButterworth(order, cutoff, 0.0, ButterworthWrapper::calcCoefLowPass);
		} else {
			return createButterworth(order, cutoff, 0.0, ButterworthWrapper::calcCoefHighPass);
		}

	}

	if (name == L"bwBandPass" || name == L"bwBandStop") {
		if (!description.has(L"freqLow")) {
			cl.error(L"freqLow is not found");
			return { };
		}
		if (!description.has(L"freqHigh")) {
			cl.error(L"freqHigh is not found");
			return { };
		}

		if (name == L"bwBandPass") {
			return createButterworth(order, cutoffLow, cutoffHigh, ButterworthWrapper::calcCoefBandPass);
		} else {
			return createButterworth(order, cutoffLow, cutoffHigh, ButterworthWrapper::calcCoefBandStop);
		}
	}

	cl.error(L"unknown filter type");
	return { };
}

FilterCascadeParser::FCF FilterCascadeParser::
createButterworth(index order, double freq1, double freq2, ButterworthParamsFunc func) {
	switch (order) {
	case 1: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<2>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	case 2: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<3>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	case 3: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<4>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	case 4: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<5>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	case 5: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<6>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	case 10: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilterFixed<11>{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	default: return [=](double sampleFrequency) {
			return std::unique_ptr<AbstractFilter>{
				new InfiniteResponseFilter{ func(order, sampleFrequency, freq1, freq2) }
			};
		};
	}
}
