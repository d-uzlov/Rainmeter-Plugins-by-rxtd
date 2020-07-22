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

namespace rxtd::audio_analyzer {
	class WeightedBlur : public ResamplerProvider {
	public:
		struct Params {
		private:
			friend WeightedBlur;

			istring sourceId;

			double radiusMultiplier;

			double minRadius;
			double maxRadius;
			double minRadiusAdaptation;
			double maxRadiusAdaptation;

			double minWeight;
		};

	private:
		struct CascadeInfo {
			struct BandInfo {
				float weight{ };
				float blurSigma{ };
			};

			std::vector<float> magnitudes;
			std::vector<BandInfo> bandsInfo;

			void setSize(index size) {
				magnitudes.resize(size);
				bandsInfo.resize(size);
			}
		};

		class GaussianCoefficientsManager {
			// radius -> coefs vector
			std::unordered_map<index, std::vector<double>> blurCoefficients;

			index minRadius{ };
			index maxRadius{ };

		public:
			const std::vector<double>& forSigma(double sigma);
			const std::vector<double>& forMaximumRadius();
			void setRadiusBounds(index min, index max);

		private:
			static std::vector<double> generateGaussianKernel(index radius);
		};

		GaussianCoefficientsManager gcm;

		Params params{ };

		index samplesPerSec{ };

		const SoundHandler* source = nullptr;
		std::vector<std::vector<float>> blurredValues;

		bool changed = true;

	public:

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl);

		void setParams(Params _params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void _process2(const DataSupplier& dataSupplier) override;
		void _finish(const DataSupplier& dataSupplier) override;

		array_view<float> getData(layer_t layer) const override;
		layer_t getLayersCount() const override;

	private:
		void blurData();
	};
}
