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

#include "undef.h"

using namespace audio_utils;

CustomizableValueTransformer::CustomizableValueTransformer(std::vector<Transformation> transformations):
	transforms(std::move(transformations)) {
	for (auto& tr : transforms) {
		if (tr.type == TransformType::eMAP) {
			tr.state.interpolator = { };

			if (std::abs(tr.args[0] - tr.args[1]) < std::numeric_limits<float>::epsilon()) {
				tr.state.interpolator.setParams(0.0, 1.0, tr.args[2], tr.args[3]);
			} else {
				tr.state.interpolator.setParams(tr.args[0], tr.args[1], tr.args[2], tr.args[3]);
			}
		}
	}
}

double CustomizableValueTransformer::apply(double value) {
	for (auto& transform : transforms) {
		switch (transform.type) {
		case TransformType::eFILTER: {
			value = transform.state.filter.next(value);
			break;
		}
		case TransformType::eDB: {
			value = std::max<double>(value, std::numeric_limits<float>::min());
			value = 10.0 * std::log10(value);
			break;
		}
		case TransformType::eMAP: {
			value = transform.state.interpolator.toValue(value);
			break;
		}
		case TransformType::eCLAMP: {
			value = std::clamp(value, transform.args[0], transform.args[1]);
			break;
		}
		default: std::terminate();
		}
	}

	return value;
}

void CustomizableValueTransformer::updateTransformations(index samplesPerSec, index blockSize) {
	for (auto& transform : transforms) {
		if (transform.type == TransformType::eFILTER) {
			transform.state.filter.setParams(
				transform.args[0] * 0.001,
				transform.args[1] * 0.001,
				samplesPerSec,
				blockSize
			);
		}
	}
}

void CustomizableValueTransformer::resetState() {
	for (auto& transform : transforms) {
		if (transform.type == TransformType::eFILTER) {
			transform.state.filter.reset();
		}
	}
}

CustomizableValueTransformer TransformationParser::parse(utils::Option transform, utils::Rainmeter::Logger& cl) {
	std::vector<Transformation> transforms;

	auto transformSequence = transform.asSequence();
	for (auto list : transformSequence) {
		const auto transformName = list.get(0).asIString();
		auto transformOpt = parseTransformation(list, cl);
		if (!transformOpt.has_value()) {
			cl.error(L"transform '{}' is not recognized, using default transform sequence", transformName);
			return { };
		}
		transforms.emplace_back(transformOpt.value());
	}

	return CustomizableValueTransformer{ transforms };
}

std::optional<TransformationParser::Transformation> TransformationParser::parseTransformation(
	utils::OptionList list,
	utils::Rainmeter::Logger& cl
) {
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
