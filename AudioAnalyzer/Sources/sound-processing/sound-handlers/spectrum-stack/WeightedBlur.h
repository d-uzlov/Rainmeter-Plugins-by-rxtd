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
	class WeightedBlur : public ResamplerProvider {
	public:
		struct Params {
			istring sourceId;

			double radiusMultiplier{ };

			double minRadius{ };
			double maxRadius{ };
			double minRadiusAdaptation{ };
			double maxRadiusAdaptation{ };

			double minWeight{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.sourceId == rhs.sourceId
					&& lhs.radiusMultiplier == rhs.radiusMultiplier
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

		bool changed = true;

	public:
		[[nodiscard]]
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr, index legacyNumber) const override;

		[[nodiscard]]
		const Params& getParams() const {
			return params;
		}

		void setParams(const Params& value) {
			params = value;
		}

	protected:
		[[nodiscard]]
		isview vGetSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vProcess(array_view<float> wave) override;
		void vFinish() override;

	private:
		void blurCascade(array_view<float> source, array_view<float> weights, array_span<float> dest, index minRadius,
		                 index maxRadius);
	};
}
