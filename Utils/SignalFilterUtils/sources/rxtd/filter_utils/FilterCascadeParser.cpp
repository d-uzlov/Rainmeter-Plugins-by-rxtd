// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "FilterCascadeParser.h"
#include "BiQuadIIR.h"
#include "BQFilterBuilder.h"
#include "InfiniteResponseFilter.h"
#include "rxtd/option_parsing/OptionMap.h"
#include "rxtd/option_parsing/OptionSequence.h"

using rxtd::filter_utils::FilterCascadeCreator;
using rxtd::filter_utils::FilterCascadeParser;
using rxtd::filter_utils::FilterCascade;
using rxtd::filter_utils::butterworth_lib::ButterworthWrapper;
using rxtd::std_fixes::StringUtils;
using rxtd::option_parsing::OptionParser;

FilterCascade FilterCascadeCreator::getInstance(double samplingFrequency) const {
	std::vector<std::unique_ptr<AbstractFilter>> result;
	for (const auto& patcher : patchers) {
		result.push_back(patcher(samplingFrequency));
	}

	FilterCascade fc{ std::move(result) };
	return fc;
}

FilterCascadeCreator FilterCascadeParser::parse(const Option& description, const Logger& logger) {
	std::vector<FCF> result;

	parser.setLogger(logger);

	for (auto element : description.asSequence(L'(', L')', L',', false, logger)) {
		result.push_back(parseFilter(element.name, element.args, logger));
	}

	return FilterCascadeCreator{ description.asString() % own(), result };
}

FilterCascadeParser::FCF
FilterCascadeParser::parseFilter(const Option& nameOpt, const Option& argsOpt, const Logger& logger) {
	auto name = nameOpt.asIString();

	if (name.empty()) {
		logger.error(L"filter name is not found", name);
		throw OptionParser::Exception{};
	}

	auto cl = logger.context(L"{}: ", name);
	const auto args = argsOpt.asMap(L',', L' ');

	if (name.startsWith(L"bq")) {
		return parseBQ(name, args, cl);
	}

	if (name.startsWith(L"bw")) {
		return parseBW(name, args, cl);
	}

	cl.error(L"unknown filter type: {}", name);
	throw OptionParser::Exception{};
}

FilterCascadeParser::FCF
FilterCascadeParser::parseBQ(isview name, const OptionMap& description, const Logger& cl) {
	if (description.get(L"q").empty()) {
		cl.error(L"Q is not found", name);
		throw OptionParser::Exception{};
	}
	if (description.get(L"freq").empty()) {
		cl.error(L"freq is not found", name);
		throw OptionParser::Exception{};
	}

	const double q = std::max<double>(parser.parse(description, L"q").as<double>(), std::numeric_limits<float>::epsilon());
	const double centralFrequency = std::max<double>(
		parser.parse(description, L"freq").as<double>(),
		std::numeric_limits<float>::epsilon()
	);
	double gain = 0.0;
	if (name == L"bqHighShelf" || name == L"bqLowShelf" || name == L"bqPeak") {
		gain = parser.parse(description, L"gain").as<double>();
	}
	const double forcedGain = parser.parse(description, L"forcedGain").valueOr(0.0);

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
		filterCreationFunc = BQFilterBuilder::createHighShelf;
	} else if (name == L"bqLowShelf") {
		filterCreationFunc = BQFilterBuilder::createLowShelf;
	} else if (name == L"bqPeak") {
		filterCreationFunc = BQFilterBuilder::createPeak;
	} else {
		cl.error(L"unknown filter type");
		throw OptionParser::Exception{};
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
FilterCascadeParser::parseBW(isview name, const OptionMap& description, const Logger& cl) {
	const index order = parser.parse(description, L"order").as<index>();
	if (order <= 0 || order > 5) {
		cl.error(L"order must be in range [1, 5] but {} found", order);
		throw OptionParser::Exception{};
	}

	const double forcedGain = parser.parse(description, L"forcedGain").valueOr(0.0);

	if (name == L"bwLowPass" || name == L"bwHighPass") {
		const double cutoff = parser.parse(description, L"freq").as<double>();

		const auto unused = description.getListOfUntouched();
		if (!unused.empty()) {
			cl.warning(L"unused options: {}", unused);
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
		const double cutoffLow = parser.parse(description, L"freqLow").as<double>();
		const double cutoffHigh = parser.parse(description, L"freqHigh").as<double>();

		const auto unused = description.getListOfUntouched();
		if (!unused.empty()) {
			cl.warning(L"unused options: {}", unused);
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
	throw OptionParser::Exception{};
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
