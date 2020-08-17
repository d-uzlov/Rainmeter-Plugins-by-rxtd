/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "ResamplerProvider.h"
#include "../../../audio-utils/GaussianCoefficientsManager.h"

namespace rxtd::audio_analyzer {
	class UniformBlur : public ResamplerProvider {
	public:
		struct Params {
			double blurRadius{ };
			double blurRadiusAdaptation{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.blurRadius == rhs.blurRadius
					&& lhs.blurRadiusAdaptation == rhs.blurRadiusAdaptation;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };
		audio_utils::GaussianCoefficientsManager gcm;
		double startingRadius{ };

	public:
		[[nodiscard]]
		bool checkSameParams(const std::any& p) const override {
			return compareParamsEquals(params, p);
		}

		void setParams(const std::any& p) override {
			params = std::any_cast<Params>(p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(Logger& cl) override;

	public:
		void vProcess(array_view<float> wave, clock::time_point killTime) override;

	private:
		void blurCascade(array_view<float> source, array_span<float> dest, index radius);
	};
}
