/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <array>
#include "RainmeterWrappers.h"
#include "filter-utils/LogarithmicIRF.h"
#include "LinearInterpolator.h"
#include "Vector2D.h"
#include "array2d_view.h"

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
			std::array<float, 2> args{ };
			utils::Vector2D<float> pastFilterValues;

			union {
				LogarithmicIRF filter{ };
				utils::LinearInterpolatorF interpolator;
			} state{ };

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
		index filterBuffersRows = 0;
		index filterBuffersColumns = 0;

	public:
		CustomizableValueTransformer() = default;
		~CustomizableValueTransformer() = default;

		explicit CustomizableValueTransformer(std::vector<Transformation> transformations);

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

		float apply(float value);

		void applyToArray(utils::array2d_span<float> values);

		void setParams(index samplesPerSec, index blockSize);

		void resetState();

	private:
		void updateFilterBuffers(index rows, index columns);
	};

	class TransformationParser {
		using Transformation = CustomizableValueTransformer::Transformation;
		using TransformType = CustomizableValueTransformer::TransformType;

	public:
		static CustomizableValueTransformer parse(utils::Option transform, utils::Rainmeter::Logger& cl);

	private:
		static std::optional<Transformation> parseTransformation(utils::OptionList list, utils::Rainmeter::Logger& cl);
	};
}
