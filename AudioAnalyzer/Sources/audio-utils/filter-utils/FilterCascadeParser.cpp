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
#include "ButterworthWrapper.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionSequence.h"

using namespace audio_utils;

FilterCascade FilterCascadeCreator::getInstance(double samplingFrequency) const {
	std::vector<std::unique_ptr<AbstractFilter>> result;
	for (const auto& patcher : patchers) {
		result.push_back(patcher(samplingFrequency));
	}

	return FilterCascade{ std::move(result) };
}

FilterCascadeCreator FilterCascadeParser::parse(const utils::OptionSequence& description) {
	std::vector<FCF> result;

	for (auto filterDescription : description) {
		auto patcher = parseFilter(filterDescription);
		if (patcher == nullptr) {
			return { };
		}

		result.push_back(std::move(patcher));
	}

	return FilterCascadeCreator{ result };
}

FilterCascadeParser::FCF FilterCascadeParser::parseFilter(const utils::OptionList& description) {
	auto name = description.get(0).asIString();

	if (name.empty()) {
		return { };
	}

	auto args = description.get(1).asMap(L',', L' ');

	if (name == L"bqHighPass") {
		if (!args.has(L"q") || !args.has(L"freq")) {
			return { };
		}

		const double q = args.get(L"q").asFloat();
		const double cutoff = args.get(L"freq").asFloat();

		if (q <= std::numeric_limits<float>::epsilon() || cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return [=](double sampleFrequency) {
			auto filter = new BiQuadIIR{ };
			*filter = BQFilterBuilder::createHighPass(q, cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}

	if (name == L"bqLowPass") {
		if (!args.has(L"q") || !args.has(L"freq")) {
			return { };
		}

		const double q = args.get(L"q").asFloat();
		const double cutoff = args.get(L"freq").asFloat();

		if (q <= std::numeric_limits<float>::epsilon() || cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return [=](double sampleFrequency) {
			auto filter = new BiQuadIIR{ };
			*filter = BQFilterBuilder::createLowPass(q, cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}

	if (name == L"bqHighShelf") {
		if (!args.has(L"q") || !args.has(L"freq") || !args.has(L"gain")) {
			return { };
		}

		const double q = args.get(L"q").asFloat();
		const double cutoff = args.get(L"freq").asFloat();
		const double gain = args.get(L"gain").asFloat();

		if (q <= std::numeric_limits<float>::epsilon() || cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return [=](double sampleFrequency) {
			auto filter = new BiQuadIIR{ };
			*filter = BQFilterBuilder::createHighShelf(gain, q, cutoff, sampleFrequency);
			if (gain > 0) {
				filter->addGain(-gain);
			}
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}

	if (name == L"bqLowShelf") {
		if (!args.has(L"q") || !args.has(L"freq") || !args.has(L"gain")) {
			return { };
		}

		const double q = args.get(L"q").asFloat();
		const double cutoff = args.get(L"freq").asFloat();
		const double gain = args.get(L"gain").asFloat();

		if (q <= std::numeric_limits<float>::epsilon() || cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return [=](double sampleFrequency) {
			auto filter = new BiQuadIIR{ };
			*filter = BQFilterBuilder::createLowShelf(gain, q, cutoff, sampleFrequency);
			if (gain > 0) {
				filter->addGain(-gain);
			}
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}

	if (name == L"bqPeak") {
		if (!args.has(L"q") || !args.has(L"freq") || !args.has(L"gain")) {
			return { };
		}

		const double q = args.get(L"q").asFloat();
		const double cutoff = args.get(L"freq").asFloat();
		const double gain = args.get(L"gain").asFloat();

		if (q <= std::numeric_limits<float>::epsilon() || cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return [=](double sampleFrequency) {
			auto filter = new BiQuadIIR{ };
			*filter = BQFilterBuilder::createPeak(gain, q, cutoff, sampleFrequency);
			if (gain > 0) {
				filter->addGain(-gain);
			}
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}

	if (name == L"bwLowPass") {
		if (!args.has(L"order") || !args.has(L"freq")) {
			return { };
		}

		const index order = args.get(L"order").asInt();
		if (order <= 0) {
			return { };
		}

		const double cutoff = args.get(L"freq").asFloat();

		if (cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return parseBWLowPass(order, cutoff);
	}

	if (name == L"bwHighPass") {
		if (!args.has(L"order") || !args.has(L"freq")) {
			return { };
		}

		const index order = args.get(L"order").asInt();
		if (order <= 0) {
			return { };
		}

		const double cutoff = args.get(L"freq").asFloat();

		if (cutoff < std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return parseBWHighPass(order, cutoff);
	}

	if (name == L"bwBandPass") {
		if (!args.has(L"order") || !args.has(L"freqLow") || !args.has(L"freqHigh")) {
			return { };
		}

		const index order = args.get(L"order").asInt();
		if (order <= 0) {
			return { };
		}

		const double cutoffLow = args.get(L"freqLow").asFloat();
		const double cutoffHigh = args.get(L"freqHigh").asFloat();

		if (cutoffLow < std::numeric_limits<float>::epsilon()
			|| cutoffHigh < cutoffLow + std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return parseBWBandPass(order, cutoffLow, cutoffHigh);
	}

	if (name == L"bwBandStop") {
		if (!args.has(L"order") || !args.has(L"freqLow") || !args.has(L"freqHigh")) {
			return { };
		}

		const index order = args.get(L"order").asInt();
		if (order <= 0) {
			return { };
		}

		const double cutoffLow = args.get(L"freqLow").asFloat();
		const double cutoffHigh = args.get(L"freqHigh").asFloat();

		if (cutoffLow < std::numeric_limits<float>::epsilon()
			|| cutoffHigh < cutoffLow + std::numeric_limits<float>::epsilon()) {
			return { };
		}

		return parseBWBandStop(order, cutoffLow, cutoffHigh);
	}

	return { };
}

FilterCascadeParser::FCF FilterCascadeParser::parseBWLowPass(index order, double cutoff) {
	switch (order) {
	case 1: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<2>{ };
			*filter = ButterworthWrapper::createLowPass<1>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 2: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<3>{ };
			*filter = ButterworthWrapper::createLowPass<2>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 3: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<4>{ };
			*filter = ButterworthWrapper::createLowPass<3>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 4: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<5>{ };
			*filter = ButterworthWrapper::createLowPass<4>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 5: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<6>{ };
			*filter = ButterworthWrapper::createLowPass<5>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 6: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<7>{ };
			*filter = ButterworthWrapper::createLowPass<6>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	default: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilter{ };
			auto [a, b] = ButterworthWrapper::calcCoefLowPass(order, cutoff, sampleFrequency);
			*filter = { a, b };
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}
}

FilterCascadeParser::FCF FilterCascadeParser::parseBWHighPass(index order, double cutoff) {
	switch (order) {
	case 1: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<2>{ };
			*filter = ButterworthWrapper::createHighPass<1>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 2: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<3>{ };
			*filter = ButterworthWrapper::createHighPass<2>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 3: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<4>{ };
			*filter = ButterworthWrapper::createHighPass<3>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 4: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<5>{ };
			*filter = ButterworthWrapper::createHighPass<4>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 5: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<6>{ };
			*filter = ButterworthWrapper::createHighPass<5>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 6: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<7>{ };
			*filter = ButterworthWrapper::createHighPass<6>(cutoff, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	default: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilter{ };
			auto [a, b] = ButterworthWrapper::calcCoefHighPass(order, cutoff, sampleFrequency);
			*filter = { a, b };
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}
}

FilterCascadeParser::FCF FilterCascadeParser::parseBWBandPass(index order, double cutoffLow, double cutoffHigh) {
	switch (order) {
	case 1: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<2>{ };
			*filter = ButterworthWrapper::createBandPass<1>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 2: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<3>{ };
			*filter = ButterworthWrapper::createBandPass<2>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 3: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<4>{ };
			*filter = ButterworthWrapper::createBandPass<3>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 4: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<5>{ };
			*filter = ButterworthWrapper::createBandPass<4>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 5: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<6>{ };
			*filter = ButterworthWrapper::createBandPass<5>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 6: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<7>{ };
			*filter = ButterworthWrapper::createBandPass<6>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	default: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilter{ };
			auto [a, b] = ButterworthWrapper::calcCoefBandPass(order, cutoffLow, cutoffHigh, sampleFrequency);
			*filter = { a, b };
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}
}

FilterCascadeParser::FCF FilterCascadeParser::parseBWBandStop(index order, double cutoffLow, double cutoffHigh) {
	switch (order) {
	case 1: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<2>{ };
			*filter = ButterworthWrapper::createBandStop<1>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 2: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<3>{ };
			*filter = ButterworthWrapper::createBandStop<2>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 3: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<4>{ };
			*filter = ButterworthWrapper::createBandStop<3>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 4: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<5>{ };
			*filter = ButterworthWrapper::createBandStop<4>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 5: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<6>{ };
			*filter = ButterworthWrapper::createBandStop<5>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	case 6: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilterFixed<7>{ };
			*filter = ButterworthWrapper::createBandStop<6>(cutoffLow, cutoffHigh, sampleFrequency);
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	default: return [=](double sampleFrequency) {
			auto filter = new InfiniteResponseFilter{ };
			auto [a, b] = ButterworthWrapper::calcCoefBandStop(order, cutoffLow, cutoffHigh, sampleFrequency);
			*filter = { a, b };
			return std::unique_ptr<AbstractFilter>{ filter };
		};
	}
}
