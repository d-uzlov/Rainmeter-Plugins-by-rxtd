/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "ResamplerProvider.h"
#include "audio-utils/GaussianCoefficientsManager.h"
#include "sound-processing/sound-handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::handler {
	class UniformBlur : public ResamplerProvider {
		struct Params {
			double blurRadius{};
			double blurRadiusAdaptation{};

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.blurRadius == rhs.blurRadius
					&& lhs.blurRadiusAdaptation == rhs.blurRadiusAdaptation;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		Params params{};
		audio_utils::GaussianCoefficientsManager gcm;
		double startingRadius{};

	public:
		[[nodiscard]]
		bool checkSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			Version version
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

	private:
		void blurCascade(array_view<float> source, array_span<float> dest, index radius);
	};
}
