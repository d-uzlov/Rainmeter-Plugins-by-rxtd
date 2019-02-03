/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "FftAnalyzer.h"

namespace rxtd::audio_analyzer {
	class BandResampler;
	class ResamplerProvider : public SoundHandler {
	public:
		virtual const BandResampler* getResampler() const = 0;
	};

	class BandResampler final : public ResamplerProvider {
	public:
		struct Params {
		private:
			friend BandResampler;

			istring fftId;
			layer_t minCascade;
			layer_t maxCascade;

			std::vector<double> bandFreqs;

			bool proportionalValues;
			bool includeDC;
		};

	private:
		struct CascadeInfo {
			std::vector<float> magnitudes;
			std::vector<float> weights;

			void setSize(index size) {
				magnitudes.resize(size);
				weights.resize(size);
			}
		};

		Params params { };

		index samplesPerSec { };

		std::vector<double> bandFreqMultipliers;
		layer_t beginCascade = 0;
		layer_t endCascade = 0;
		index bandsCount = 0;

		std::vector<CascadeInfo> cascadesInfo;

		bool changed = true;
		bool valid = false;

		mutable string propString { };


	public:
		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl, utils::Rainmeter& rain);

		void setParams(Params _params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;


		bool isValid() const override {
			return valid;
		}
		array_view<float> getData(layer_t layer) const override;
		layer_t getLayersCount() const override;

		const wchar_t* getProp(const isview& prop) const override;

		const BandResampler* getResampler() const override {
			return this;
		}
		layer_t getStartingLayer() const override {
			return beginCascade;
		}
		layer_t getEndCascade() const {
			return endCascade;
		}

		array_view<float> getBandWeights(layer_t cascade) const;

	private:
		void updateValues(const DataSupplier& dataSupplier);
		void computeBandInfo(const FftAnalyzer& source, layer_t startCascade, layer_t endCascade);
		void sampleData(const FftAnalyzer& source, layer_t startCascade, layer_t endCascade);

		static std::optional<std::vector<double>> parseFreqList(const utils::OptionParser::OptionList& bounds, utils::Rainmeter::ContextLogger& cl, const utils::Rainmeter& rain);
	};
}
