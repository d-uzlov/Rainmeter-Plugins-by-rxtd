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
		auto transformOpt = parseTransformation(list, cl);
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
	const index paramCount = list.size() - 1;

	if (transformName == L"filter") {
		tr.type = TransformType::eFILTER;

		if (paramCount == 0 || paramCount > 2) {
			cl.error(L"'filter' must have 1 or 2 parameters but {} found", paramCount);
			return std::nullopt;
		}
		tr.args[0] = list.get(1).asFloat();
		tr.args[1] = paramCount == 1 ? tr.args[0] : list.get(2).asFloat(tr.args[0]);
	} else if (transformName == L"db") {
		tr.type = TransformType::eDB;

		if (paramCount != 0) {
			cl.error(L"'db' must have 0 parameters but {} found", paramCount);
			return std::nullopt;
		}
	} else if (transformName == L"map") {
		tr.type = TransformType::eMAP;

		std::array<double, 4> args{ };
		if (paramCount == 2) {
			args[2] = 0.0;
			args[3] = 1.0;
		} else if (paramCount == 4) {
			for (int i = 1; i < list.size(); ++i) {
				args[i - 1] = list.get(i).asFloat();
			}
		} else {
			cl.error(L"'map' must have 2 or 4 parameters but {} found", paramCount);
			return std::nullopt;
		}

		if (std::abs(args[0] - args[1]) < std::numeric_limits<float>::epsilon()) {
			return std::nullopt;
		}

		tr.state.interpolator.setParams(args[0], args[1], args[2], args[3]);

	} else if (transformName == L"clamp") {
		tr.type = TransformType::eCLAMP;

		if (paramCount == 0) {
			tr.args[0] = 0.0;
			tr.args[1] = 1.0;
		} else if (paramCount == 2) {
			for (int i = 1; i < list.size(); ++i) {
				tr.args[i - 1] = list.get(i).asFloat();
			}
		} else {
			cl.error(L"'clamp' must have 0 or 2 parameters but {} found", paramCount);
			return std::nullopt;
		}
	} else {
		cl.error(L"'{}' is not recognized as transform type", transformName);
		return std::nullopt;
	}

	return tr;
}
