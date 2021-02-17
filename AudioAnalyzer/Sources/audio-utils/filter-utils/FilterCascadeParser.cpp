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
#include "option-parsing/OptionMap.h"
#include "option-parsing/OptionSequence.h"
#include "../butterworth-lib/ButterworthWrapper.h"

using namespace rxtd::audio_utils;

FilterCascade FilterCascadeCreator::getInstance(double samplingFrequency) const {
	std::vector<std::unique_ptr<AbstractFilter>> result;
	for (const auto& patcher : patchers) {
		result.push_back(patcher(samplingFrequency));
	}

	FilterCascade fc{ std::move(result) };
	return fc;
}

FilterCascadeCreator FilterCascadeParser::parse(const Option& description, Logger& logger) {
	std::vector<FCF> result;

	for (auto filterDescription : description.asSequence()) {
		auto patcher = parseFilter(filterDescription, logger);
		if (patcher == nullptr) {
			return {};
		}

		result.push_back(std::move(patcher));
	}

	return FilterCascadeCreator{ description.asString() % own(), result };
}

FilterCascadeParser::FCF
FilterCascadeParser::parseFilter(const OptionList& description, Logger& logger) {
	auto name = description.get(0).asIString();

	if (name.empty()) {
		logger.error(L"filter name is not found", name);
		return {};
	}

	auto cl = logger.context(L"'{}': ", name);
	const auto args = description.get(1).asMap(L',', L' ');

	if (utils::StringUtils::checkStartsWith(name, L"bq")) {
		return parseBQ(name, args, cl);
	}

	if (utils::StringUtils::checkStartsWith(name, L"bw")) {
		return parseBW(name, args, cl);
	}

	cl.error(L"unknown filter type");
	return {};
}

FilterCascadeParser::FCF
FilterCascadeParser::parseBQ(isview name, const OptionMap& description, Logger& cl) {
	if (description.get(L"q").empty()) {
		cl.error(L"Q is not found", name);
		return {};
	}
	if (description.get(L"freq").empty()) {
		cl.error(L"freq is not found", name);
		return {};
	}

	const double q = std::max<double>(description.get(L"q").asFloat(), std::numeric_limits<float>::epsilon());
	const double centralFrequency = std::max<double>(
		description.get(L"freq").asFloat(),
		std::numeric_limits<float>::epsilon()
	);
	double gain = 0.0;
	if (name == L"bqHighShelf" || name == L"bqLowShelf" || name == L"bqPeak") {
		gain = description.get(L"gain").asFloat();
	}
	const double forcedGain = description.get(L"forcedGain").asFloat();

	const auto unused = description.getListOfUntouched();
	if (!unused.empty()) {
		cl.warning(L"unused options: {}", unused);
	}

	using FilterCreationFunc = BiQuadIIR (*)(double samplingFrequency, double q, double freq, double dbGain);
	FilterCreationFunc filterCreationFunc;

	if (name == L"bqHighPass") {
		filterCreationFunc = BQFilterBuilder::createHighPass;
	} else if (name == L"bqLowPass") {
		filterCreationFunc = BQFilterBuilder::createLowPass;
	} else if (name == L"bqHighShelf") {
		if (description.get(L"gain").empty()) {
			cl.error(L"gain is not found", name);
			return {};
		}

		filterCreationFunc = BQFilterBuilder::createHighShelf;
	} else if (name == L"bqLowShelf") {
		if (description.get(L"gain").empty()) {
			cl.error(L"gain is not found", name);
			return {};
		}

		filterCreationFunc = BQFilterBuilder::createLowShelf;
	} else if (name == L"bqPeak") {
		if (description.get(L"gain").empty()) {
			cl.error(L"gain is not found", name);
			return {};
		}

		filterCreationFunc = BQFilterBuilder::createPeak;
	} else {
		cl.error(L"unknown filter type");
		return {};
	}

	return [=](double sampleFrequency) {
		auto filter = new BiQuadIIR{};
		*filter = filterCreationFunc(sampleFrequency, q, centralFrequency, gain);
		if (gain > 0.0) {
			filter->addGainDbEnergy(-gain);
		}
		filter->addGainDbEnergy(forcedGain);
		return std::unique_ptr<AbstractFilter>{ filter };
	};
}

FilterCascadeParser::FCF
FilterCascadeParser::parseBW(isview name, const OptionMap& description, Logger& cl) {
	if (description.get(L"order").empty()) {
		cl.error(L"order is not found");
		return {};
	}

	const index order = description.get(L"order").asInt();
	if (order <= 0 || order > 5) {
		cl.error(L"order must be in range [1, 5] but {} found", order);
		return {};
	}

	const double cutoff = description.get(L"freq").asFloat();
	const double cutoffLow = description.get(L"freqLow").asFloat();
	const double cutoffHigh = description.get(L"freqHigh").asFloat();

	const double forcedGain = description.get(L"forcedGain").asFloat();

	const auto unused = description.getListOfUntouched();
	if (!unused.empty()) {
		cl.warning(L"unused options: {}", unused);
	}

	if (name == L"bwLowPass" || name == L"bwHighPass") {
		if (description.get(L"freq").empty()) {
			cl.error(L"freq is not found");
			return {};
		}

		if (name == L"bwLowPass") {
			return createButterworth<ButterworthWrapper::oneSideSlopeSize>(
				order, forcedGain, cutoff, 0.0, ButterworthWrapper::lowPass
			);
		} else {
			return createButterworth<ButterworthWrapper::oneSideSlopeSize>(
				order, forcedGain, cutoff, 0.0, ButterworthWrapper::highPass
			);
		}

	}

	if (name == L"bwBandPass" || name == L"bwBandStop") {
		if (description.get(L"freqLow").empty()) {
			cl.error(L"freqLow is not found");
			return {};
		}
		if (description.get(L"freqHigh").empty()) {
			cl.error(L"freqHigh is not found");
			return {};
		}

		if (name == L"bwBandPass") {
			return createButterworth<ButterworthWrapper::twoSideSlopeSize>(
				order, forcedGain, cutoffLow, cutoffHigh, ButterworthWrapper::bandPass
			);
		} else {
			return createButterworth<ButterworthWrapper::twoSideSlopeSize>(
				order, forcedGain, cutoffLow, cutoffHigh, ButterworthWrapper::bandStop
			);
		}
	}

	cl.error(L"unknown filter type");
	return {};
}

template<rxtd::index size>
FilterCascadeParser::FCF FilterCascadeParser::createButterworthMaker(
	index order, double forcedGain, double freq1,
	double freq2,
	const ButterworthWrapper::GenericCoefCalculator&
	butterworthMaker
) {
	return [=](double sampleFrequency) {
		auto ptr = new InfiniteResponseFilterFixed<size>{
			butterworthMaker.calcCoef(order, sampleFrequency, freq1, freq2)
		};
		ptr->addGainDbEnergy(forcedGain);
		return std::unique_ptr<AbstractFilter>{ ptr };
	};
}

template<ButterworthWrapper::SizeFuncSignature sizeFunc>
FilterCascadeParser::FCF FilterCascadeParser::createButterworth(
	index order, double forcedGain, double freq1,
	double freq2,
	const ButterworthWrapper::GenericCoefCalculator&
	butterworthMaker
) {
	switch (order) {
	case 1: return createButterworthMaker<sizeFunc(1)>(order, forcedGain, freq1, freq2, butterworthMaker);
	case 2: return createButterworthMaker<sizeFunc(2)>(order, forcedGain, freq1, freq2, butterworthMaker);
	case 3: return createButterworthMaker<sizeFunc(3)>(order, forcedGain, freq1, freq2, butterworthMaker);
	case 4: return createButterworthMaker<sizeFunc(4)>(order, forcedGain, freq1, freq2, butterworthMaker);
	case 5: return createButterworthMaker<sizeFunc(5)>(order, forcedGain, freq1, freq2, butterworthMaker);

	default: return [=](double sampleFrequency) {
			auto ptr = new InfiniteResponseFilter{
				butterworthMaker.calcCoef(order, sampleFrequency, freq1, freq2)
			};
			ptr->addGainDbEnergy(forcedGain);
			return std::unique_ptr<AbstractFilter>{ ptr };
		};
	}
}
