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
	class UniformBlur : public ResamplerProvider {
	public:
		struct Params {
		private:
			friend UniformBlur;

			istring sourceId;

			double blurRadius;
			double blurRadiusAdaptation;
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
		public:
			const std::vector<double>& forRadius(index radius);

		private:
			static std::vector<double> generateGaussianKernel(index radius);
		};

		GaussianCoefficientsManager gcm;

		Params params{ };

		index samplesPerSec{ };

		SoundHandler* source = nullptr;

		std::vector<std::vector<float>> blurredValues;

		bool changed = true;

		std::vector<LayerData> layers;

	public:
		static std::optional<Params> parseParams(const OptionMap& optionMap, Logger& cl);

		void setParams(const Params& params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void _process(const DataSupplier& dataSupplier) override;
		void _finish() override;

		LayeredData getData() const override {
			return layers;
		}

	protected:
		[[nodiscard]]
		isview getSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		bool vCheckSources(Logger& cl) override;

	private:
		void blurData(const SoundHandler& resampler);
	};
}
