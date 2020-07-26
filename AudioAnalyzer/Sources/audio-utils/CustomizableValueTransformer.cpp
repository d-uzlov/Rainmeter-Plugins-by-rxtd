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

#include "undef.h"

using namespace audio_utils;

CustomizableValueTransformer::CustomizableValueTransformer(std::vector<Transformation> transformations):
	transforms(std::move(transformations)) {
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

void CustomizableValueTransformer::applyToArray(utils::array2d_span<float> values) {
	const index rowsCount = values.getBuffersCount();
	const index columnsCount = values.getBufferSize();
	if (filterBuffersRows != rowsCount || filterBuffersColumns != columnsCount) {
		updateFilterBuffers(rowsCount, columnsCount);
	}

	for (int row = 0; row < rowsCount; ++row) {
		for (int column = 0; column < columnsCount; ++column) {
			double value = values[row][column];

			for (auto& transform : transforms) {
				switch (transform.type) {
				case TransformType::eFILTER: {
					const float prev = transform.pastFilterValues[row][column];
					value = transform.state.filter.apply(prev, value);
					transform.pastFilterValues[row][column] = value;
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

			values[row][column] = value;
		}
	}
}

void CustomizableValueTransformer::setParams(index samplesPerSec, index blockSize) {
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

void CustomizableValueTransformer::updateFilterBuffers(index rows, index columns) {
	for (auto& transform : transforms) {
		if (transform.type == TransformType::eFILTER) {
			transform.pastFilterValues.setBuffersCount(rows);
			transform.pastFilterValues.setBufferSize(columns);
			transform.pastFilterValues.init(0.0);
		}
	}

	filterBuffersRows = rows;
	filterBuffersColumns = columns;
}

CustomizableValueTransformer TransformationParser::parse(utils::Option transform, utils::Rainmeter::Logger& cl) {
	std::vector<Transformation> transforms;

	auto transformSequence = transform.asSequence();
	for (auto list : transformSequence) {
		auto logger = cl.context(L"{}: ", list.get(0).asString());
		auto transformOpt = parseTransformation(list, logger);
		if (!transformOpt.has_value()) {
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

	utils::OptionMap params;
	if (list.size() >= 2) {
		params = list.get(1).asMap(L',', L' ');
	}

	if (transformName == L"filter") {
		tr.type = TransformType::eFILTER;

		tr.args[0] = params.get(L"a").asFloat();
		tr.args[1] = params.get(L"d").asFloat(tr.args[0]);
	} else if (transformName == L"db") {
		tr.type = TransformType::eDB;
	} else if (transformName == L"map") {
		tr.type = TransformType::eMAP;

		double linMin;
		double linMax;
		if (params.has(L"from")) {
			auto range = params.get(L"from").asList(L';');
			if (range.size() != 2) {
				cl.error(L"need 2 params for source range but {} found", range.size());
				return std::nullopt;
			}
			linMin = range.get(0).asFloat();
			linMax = range.get(1).asFloat();

			if (std::abs(linMin - linMax) < std::numeric_limits<float>::epsilon()) {
				cl.error(L"source range is too small: {} and {}", linMin, linMax);
				return std::nullopt;
			}
		} else {
			cl.error(L"source range is not found");
			return std::nullopt;
		}
		double valMin = 0.0;
		double valMax = 1.0;

		if (params.has(L"to")) {
			auto range = params.get(L"to").asList(L';');
			if (range.size() != 2) {
				cl.error(L"need 2 params for target range but {} found", range.size());
				return std::nullopt;
			}
			valMin = range.get(0).asFloat();
			valMax = range.get(1).asFloat();
		}

		tr.state.interpolator.setParams(linMin, linMax, valMin, valMax);

	} else if (transformName == L"clamp") {
		tr.type = TransformType::eCLAMP;

		tr.args[0] = params.get(L"min").asFloat(0.0);
		tr.args[1] = params.get(L"min").asFloat(1.0);
	} else {
		cl.error(L"'{}' is not recognized as a transform type", transformName);
		return std::nullopt;
	}

	return tr;
}
