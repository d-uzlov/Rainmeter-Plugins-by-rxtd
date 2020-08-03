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
#include "Vector2D.h"

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
		// todo unite with uniform blur GCM
		class GaussianCoefficientsManager {
			// radius -> coefs vector
			std::unordered_map<index, std::vector<double>> blurCoefficients;

			index minRadius{ };
			index maxRadius{ };

		public:
			array_view<double> forSigma(double sigma) {
				const auto radius = std::clamp<index>(std::lround(sigma * 3.0), minRadius, maxRadius);

				auto& vec = blurCoefficients[radius];
				if (vec.empty()) {
					vec = generateGaussianKernel(radius);
				}

				return vec;
			}

			array_view<double> forMaximumRadius() {
				auto& vec = blurCoefficients[maxRadius];
				if (vec.empty()) {
					vec = generateGaussianKernel(maxRadius);
				}

				return vec;
			}

			void setRadiusBounds(index min, index max) {
				minRadius = min;
				maxRadius = max;
			}

		private:
			static std::vector<double> generateGaussianKernel(index radius);
		};

		GaussianCoefficientsManager gcm;

		Params params{ };

		BandResampler* resamplerPtr{ };

		bool changed = true;

	public:
		[[nodiscard]]
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const override;

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
		void vProcess(const DataSupplier& dataSupplier) override;
		void vFinish() override;

	private:
		void blurCascade(array_view<float> source, array_view<float> weights, array_span<float> dest);
	};
}
