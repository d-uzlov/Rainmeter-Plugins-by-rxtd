/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CustomizableValueTransformer.h"

#include "option-parser/OptionList.h"
#include "option-parser/OptionSequence.h"
#include "option-parser/OptionMap.h"

#include "MyMath.h"

using namespace audio_utils;

double CustomizableValueTransformer::apply(double value) {
	for (auto& transform : transforms) {
		switch (transform.type) {
		case TransformType::eDB: {
			value = std::max<double>(value, 0.0);
			value = 10.0f * std::log10(value);
			break;
		}
		case TransformType::eMAP: {
			value = transform.interpolator.toValue(float(value));
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

void CustomizableValueTransformer::applyToArray(array_view<float> source, array_span<float> dest) {
	const auto logCoef = float(std::log10(2)); // == log(2) / log(10)

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
					value = 10.0f * utils::MyMath::fastLog2(value) * logCoef;
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

CustomizableValueTransformer
CustomizableValueTransformer::parse(sview transformDescription, utils::Rainmeter::Logger& cl) {
	std::vector<TransformationInfo> transforms;

	for (auto list : utils::Option{ transformDescription }.asSequence()) {
		auto logger = cl.context(L"{}: ", list.get(0).asString());
		auto transformOpt = parseTransformation(list, logger);
		if (!transformOpt.has_value()) {
			return {};
		}
		transforms.emplace_back(transformOpt.value());
	}

	return CustomizableValueTransformer{ transforms };
}

std::optional<CustomizableValueTransformer::TransformationInfo> CustomizableValueTransformer::parseTransformation(
	utils::OptionList list,
	utils::Rainmeter::Logger& cl
) {
	const auto transformName = list.get(0).asIString();
	TransformationInfo tr{};

	utils::OptionMap params;
	if (list.size() >= 2) {
		params = list.get(1).asMap(L',', L' ');
	}

	if (transformName == L"db") {
		tr.type = TransformType::eDB;
	} else if (transformName == L"map") {
		tr.type = TransformType::eMAP;

		float linMin;
		float linMax;
		if (params.has(L"from")) {
			auto range = params.get(L"from").asList(L':');
			if (range.size() != 2) {
				cl.error(L"need 2 params for source range but {} found", range.size());
				return {};
			}
			linMin = range.get(0).asFloatF();
			linMax = range.get(1).asFloatF();

			if (std::abs(linMin - linMax) < std::numeric_limits<float>::epsilon()) {
				cl.error(L"source range is too small: {} and {}", linMin, linMax);
				return {};
			}
		} else {
			cl.error(L"source range is not found");
			return {};
		}

		float valMin = 0.0;
		float valMax = 1.0;

		if (params.has(L"to")) {
			auto range = params.get(L"to").asList(L':');
			if (range.size() != 2) {
				cl.error(L"need 2 params for target range but {} found", range.size());
				return std::nullopt;
			}
			valMin = range.get(0).asFloatF();
			valMax = range.get(1).asFloatF();
		}

		tr.interpolator.setParams(linMin, linMax, valMin, valMax);

	} else if (transformName == L"clamp") {
		tr.type = TransformType::eCLAMP;

		tr.args[0] = params.get(L"min").asFloatF(0.0f);
		tr.args[1] = params.get(L"max").asFloatF(1.0f);

		tr.args[0] = std::min(tr.args[0], tr.args[1]);
		tr.args[1] = std::max(tr.args[0], tr.args[1]);
	} else {
		cl.error(L"'{}' is not recognized as a transform type", transformName);
		return {};
	}

	return tr;
}
