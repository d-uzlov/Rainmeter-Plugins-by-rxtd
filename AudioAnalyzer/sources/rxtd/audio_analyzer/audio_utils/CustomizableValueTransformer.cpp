/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CustomizableValueTransformer.h"

#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/option_parsing/OptionMap.h"
#include "rxtd/option_parsing/OptionSequence.h"
#include "rxtd/std_fixes/MathBitTwiddling.h"

using CVT = rxtd::audio_analyzer::audio_utils::CustomizableValueTransformer;
using rxtd::std_fixes::MathBitTwiddling;
using rxtd::option_parsing::Option;
using rxtd::option_parsing::OptionMap;

double CVT::apply(double value) {
	for (auto& transform : transforms) {
		switch (transform.type) {
		case TransformType::eDB: {
			value = std::max<double>(value, 0.0);
			value = 10.0 * std::log10(value);
			break;
		}
		case TransformType::eMAP: {
			value = transform.interpolator.toValue(static_cast<float>(value));
			break;
		}
		case TransformType::eCLAMP: {
			value = std::clamp<double>(value, transform.args[0], transform.args[1]);
			break;
		}
		}
	}

	return value;
}

void CVT::applyToArray(array_view<float> source, array_span<float> dest) {
	const auto logCoef = std::log10(2.0f); // == log(2) / log(10)

	for (auto& transform : transforms) {
		switch (transform.type) {
		case TransformType::eDB: {
			auto sourceIter = source.begin();
			auto destIter = dest.begin();

			for (; sourceIter != source.end();
			       ++sourceIter, ++destIter) {
				float value = *sourceIter;
				float& destValue = *destIter;

				if (value <= 0.0f) {
					destValue = -std::numeric_limits<float>::infinity();
				} else {
					value = std::max(value, 0.0f);
					value = 10.0f * MathBitTwiddling::fastLog2(value) * logCoef;
					// value = 10.0 * std::log10(value);
					destValue = value;
				}
			}

			break;
		}
		case TransformType::eMAP: {
			auto sourceIter = source.begin();
			auto destIter = dest.begin();

			for (; sourceIter != source.end();
			       ++sourceIter, ++destIter) {
				float value = *sourceIter;
				float& destValue = *destIter;

				value = transform.interpolator.toValue(value);

				destValue = value;
			}

			break;
		}
		case TransformType::eCLAMP: {
			auto sourceIter = source.begin();
			auto destIter = dest.begin();

			for (; sourceIter != source.end();
			       ++sourceIter, ++destIter) {
				float value = *sourceIter;
				float& destValue = *destIter;

				value = std::clamp(value, transform.args[0], transform.args[1]);

				destValue = value;
			}

			break;
		}
		}

		source = dest;
	}
}

CVT CVT::parse(sview transformDescription, option_parsing::OptionParser parser, Logger& cl) {
	std::vector<TransformationInfo> transforms;


	for (auto list : Option{ transformDescription }.asSequence()) {
		auto logger = cl.context(L"{}: ", list.get(0).asString());
		auto transformOpt = parseTransformation(list, parser, logger);
		if (!transformOpt.has_value()) {
			return {};
		}
		transforms.emplace_back(transformOpt.value());
	}

	return CustomizableValueTransformer{ transforms };
}

std::optional<CVT::TransformationInfo> CVT::parseTransformation(OptionList list, option_parsing::OptionParser parser, Logger& cl) {
	const auto transformName = list.get(0).asIString();
	TransformationInfo tr{};

	OptionMap params;
	if (list.size() >= 2) {
		params = list.get(1).asMap(L',', L' ');
	}

	if (transformName == L"db") {
		tr.type = TransformType::eDB;
	} else if (transformName == L"map") {
		tr.type = TransformType::eMAP;

		auto sourceRangeOpt = params.get(L"from");
		if (sourceRangeOpt.empty()) {
			cl.error(L"source range is not found");
			return {};
		}

		auto sourceRange = sourceRangeOpt.asList(L':');
		if (sourceRange.size() != 2) {
			cl.error(L"need 2 params for source range but {} found", sourceRange.size());
			return {};
		}

		float linMin = parser.parseFloatF(sourceRange.get(0));
		float linMax = parser.parseFloatF(sourceRange.get(1));

		if (std::abs(linMin - linMax) < std::numeric_limits<float>::epsilon()) {
			cl.error(L"source range is too small: {} and {}", linMin, linMax);
			return {};
		}

		float valMin = 0.0;
		float valMax = 1.0;

		if (auto destRange = params.get(L"to");
			!destRange.empty()) {
			auto range = destRange.asList(L':');
			if (range.size() != 2) {
				cl.error(L"need 2 params for target range but {} found", range.size());
				return std::nullopt;
			}
			valMin = parser.parseFloatF(range.get(0));
			valMax = parser.parseFloatF(range.get(1));
		}

		tr.interpolator.setParams(linMin, linMax, valMin, valMax);

	} else if (transformName == L"clamp") {
		tr.type = TransformType::eCLAMP;

		tr.args[0] = parser.parseFloatF(params.get(L"min"), 0.0f);
		tr.args[1] = parser.parseFloatF(params.get(L"max"), 1.0f);

		tr.args[0] = std::min(tr.args[0], tr.args[1]);
		tr.args[1] = std::max(tr.args[0], tr.args[1]);
	} else {
		cl.error(L"'{}' is not recognized as a transform type", transformName);
		return {};
	}

	return tr;
}
