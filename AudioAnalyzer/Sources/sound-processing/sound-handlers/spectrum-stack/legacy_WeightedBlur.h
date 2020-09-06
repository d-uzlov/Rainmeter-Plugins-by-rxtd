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
#include "BandResampler.h"
#include "ResamplerProvider.h"
#include "../../../audio-utils/GaussianCoefficientsManager.h"

namespace rxtd::audio_analyzer {
	class legacy_WeightedBlur : public ResamplerProvider {
	public:
		struct Params {
			double radiusMultiplier{ };

			double minRadius{ };
			double maxRadius{ };
			double minRadiusAdaptation{ };
			double maxRadiusAdaptation{ };

			double minWeight{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.radiusMultiplier == rhs.radiusMultiplier
					&& lhs.minRadius == rhs.minRadius
					&& lhs.maxRadius == rhs.maxRadius
					&& lhs.minRadiusAdaptation == rhs.minRadiusAdaptation
					&& lhs.maxRadiusAdaptation == rhs.maxRadiusAdaptation
					&& lhs.minWeight == rhs.minWeight;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		audio_utils::GaussianCoefficientsManager gcm;

		Params params{ };

		BandResampler* resamplerPtr{ };

	public:
		[[nodiscard]]
		bool checkSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

	private:
		void blurCascade(
			array_view<float> source, array_view<float> weights, array_span<float> dest,
			index minRadius, index maxRadius
		);
	};
}
