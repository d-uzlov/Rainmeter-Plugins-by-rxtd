/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <utility>
#include <array>
#include "RainmeterWrappers.h"
#include "option-parser/OptionList.h"
#include "option-parser/OptionSequence.h"
#include "LogarithmicIRF.h"
#include "LinearInterpolator.h"

namespace rxtd::audio_utils {
	class CustomizableValueTransformer {
	public:
		enum class TransformType {
			eFILTER,
			eDB,
			eMAP,
			eCLAMP,
		};

		struct Transformation {
			TransformType type{ };
			std::array<double, 4> args{ };
			mutable void* state;

			~Transformation() {
				if (state != nullptr) {
					switch (type) {
					case TransformType::eFILTER:
						delete static_cast<LogarithmicIRF*>(state);
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

			// autogenerated
			friend bool operator==(const Transformation& lhs, const Transformation& rhs) {
				return lhs.type == rhs.type;
			}

			friend bool operator!=(const Transformation& lhs, const Transformation& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		std::vector<Transformation> transforms;

	public:
		CustomizableValueTransformer() = default;
		~CustomizableValueTransformer() = default;

		explicit CustomizableValueTransformer(std::vector<Transformation> transformations) :
			transforms(std::move(transformations)) {
			for (auto& tr : transforms) {
				if (tr.type == TransformType::eMAP) {
					auto* interpolator = new utils::LinearInterpolator{ };
					tr.state = interpolator;

					if (std::abs(tr.args[0] - tr.args[1]) < std::numeric_limits<float>::epsilon()) {
						interpolator->setParams(0.0, 1.0, tr.args[2], tr.args[3]);
					} else {
						interpolator->setParams(tr.args[0], tr.args[1], tr.args[2], tr.args[3]);
					}
				}
			}
		}

		CustomizableValueTransformer(const CustomizableValueTransformer& other) = default;
		CustomizableValueTransformer(CustomizableValueTransformer&& other) noexcept = default;
		CustomizableValueTransformer& operator=(const CustomizableValueTransformer& other) = default;
		CustomizableValueTransformer& operator=(CustomizableValueTransformer&& other) noexcept = default;

		// autogenerated
		friend bool operator==(const CustomizableValueTransformer& lhs, const CustomizableValueTransformer& rhs) {
			return lhs.transforms == rhs.transforms;
		}

		friend bool operator!=(const CustomizableValueTransformer& lhs, const CustomizableValueTransformer& rhs) {
			return !(lhs == rhs);
		}

		bool isEmpty() const {
			return transforms.empty();
		}

		double apply(double value) {
			for (const auto& transform : transforms) {
				switch (transform.type) {
				case TransformType::eFILTER: {
					auto& filter = *static_cast<LogarithmicIRF*>(transform.state);
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

			return value;
		}

		void updateTransformations(index samplesPerSec, index blockSize) {
			for (const auto& transform : transforms) {
				if (transform.type == TransformType::eFILTER) {
					if (transform.state == nullptr) {
						transform.state = new LogarithmicIRF { };
					}
					auto& filter = *static_cast<LogarithmicIRF*>(transform.state);
					filter.setParams(transform.args[0] * 0.001, transform.args[1] * 0.001, samplesPerSec, blockSize);
				}
			}
		}

		void resetState() {
			for (const auto& transform : transforms) {
				if (transform.type == TransformType::eFILTER) {
					auto filter = static_cast<LogarithmicIRF*>(transform.state);
					filter->reset();
				}
			}
		}
	};

	class TransformationParser {
		using Transformation = CustomizableValueTransformer::Transformation;
		using TransformType = CustomizableValueTransformer::TransformType;

	public:
		static CustomizableValueTransformer parse(utils::Option transform, utils::Rainmeter::Logger& cl) {
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

	private:
		static std::optional<Transformation> parseTransformation(utils::OptionList list, utils::Rainmeter::Logger& cl) {
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
	};
}
