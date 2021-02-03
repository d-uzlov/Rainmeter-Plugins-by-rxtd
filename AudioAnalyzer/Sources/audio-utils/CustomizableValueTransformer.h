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
#include "rainmeter/Logger.h"
#include "LinearInterpolator.h"
#include "option-parsing/OptionList.h"

namespace rxtd::audio_utils {
	class CustomizableValueTransformer {
	public:
		using Logger = ::rxtd::common::rainmeter::Logger;
		using OptionList = ::rxtd::common::options::OptionList;

		enum class TransformType {
			eDB,
			eMAP,
			eCLAMP,
		};

		struct TransformationInfo {
			TransformType type{};
			std::array<float, 2> args{};

			utils::LinearInterpolator<float> interpolator;

			// autogenerated
			friend bool operator==(const TransformationInfo& lhs, const TransformationInfo& rhs) {
				return lhs.type == rhs.type
					&& lhs.args == rhs.args;
			}

			friend bool operator!=(const TransformationInfo& lhs, const TransformationInfo& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		std::vector<TransformationInfo> transforms;

	public:
		CustomizableValueTransformer() = default;

		explicit CustomizableValueTransformer(std::vector<TransformationInfo> transformations) :
			transforms(std::move(transformations)) { }

		// autogenerated
		friend bool operator==(const CustomizableValueTransformer& lhs, const CustomizableValueTransformer& rhs) {
			return lhs.transforms == rhs.transforms;
		}

		friend bool operator!=(const CustomizableValueTransformer& lhs, const CustomizableValueTransformer& rhs) {
			return !(lhs == rhs);
		}

		[[nodiscard]]
		bool isEmpty() const {
			return transforms.empty();
		}

		[[nodiscard]]
		double apply(double value);

		[[nodiscard]]
		float apply(float value) {
			return float(apply(double(value)));
		}

		void applyToArray(array_view<float> source, array_span<float> dest);

		[[nodiscard]]
		static CustomizableValueTransformer parse(sview transformDescription, Logger& cl);

	private:
		[[nodiscard]]
		static std::optional<TransformationInfo>
		parseTransformation(OptionList list, Logger& cl);
	};
}
