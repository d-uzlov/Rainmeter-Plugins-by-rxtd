/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "FftAnalyzer.h"
#include "../SoundHandler.h"
#include "ResamplerProvider.h"

namespace rxtd::audio_analyzer {
	class BandResampler final : public ResamplerProvider {
	public:
		struct Params {
		private:
			friend BandResampler;

			istring fftId;
			index minCascade{ };
			index maxCascade{ };

			std::vector<float> bandFreqs;

			bool legacy_proportionalValues{ };
			bool includeDC{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.fftId == rhs.fftId
					&& lhs.minCascade == rhs.minCascade
					&& lhs.maxCascade == rhs.maxCascade
					&& lhs.bandFreqs == rhs.bandFreqs
					&& lhs.legacy_proportionalValues == rhs.legacy_proportionalValues
					&& lhs.includeDC == rhs.includeDC;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		FftAnalyzer* fftSource = nullptr;

		std::vector<float> legacy_bandFreqMultipliers{ };
		utils::Vector2D<float> layerWeights;
		utils::Vector2D<float> bandWeights;

		index startCascade = 0;
		index endCascade = 0;
		index bandsCount = 0;

		bool changed = true;

	public:
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const override;

		const Params& getParams() const {
			return params;
		}

		void setParams(const Params& value);

	private:
		static std::vector<float> parseFreqList(sview listId, const Rainmeter& rain);

	protected:
		[[nodiscard]]
		isview vGetSourceName() const override {
			return params.fftId;
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vProcess(const DataSupplier& dataSupplier) override;
		void vFinish() override;

		[[nodiscard]]
		index getStartingLayer() const override {
			return startCascade;
		}

		[[nodiscard]]
		index getEndCascade() const {
			return endCascade;
		}

		[[nodiscard]]
		array_view<float> getLayerWeights(index cascade) const {
			return layerWeights[cascade];
		}

		[[nodiscard]]
		array_view<float> getBandWeights(index band) const {
			return bandWeights[band];
		}

		BandResampler* getResampler() override {
			return this;
		}

		bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const override;

	private:
		void sampleCascade(array_view<float> source, array_span<float> dest, double binWidth);

		// depends on fft size and sample rate
		void computeWeights(index fftSize);
		void computeCascadeWeights(array_span<float> result, index fftBinsCount, double binWidth);

		void legacy_generateBandMultipliers();
	};
}
