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
#include "BandResampler.h"

namespace rxtd::audio_analyzer {
	class BandCascadeTransformer : public SoundHandler {
	public:
		enum class MixFunction {
			AVERAGE,
			PRODUCT,
		};
		struct Params {
		private:
			friend BandCascadeTransformer;

			istring sourceId;

			double minWeight;
			double targetWeight;
			double weightFallback;

			double zeroLevel;
			double zeroLevelHard;
			double zeroWeight;

			MixFunction mixFunction;
		};

	private:
		Params params { };

		index samplesPerSec { };

		std::vector<float> resultValues;

		bool changed = true;
		bool valid = false;
		bool analysisComputed = false;

		mutable string propString { };

		struct {
			string analysisString { };
			layer_t minCascadeUsed = -1;
			layer_t maxCascadeUsed = -1;
			std::vector<layer_t> bandEndCascades;
			bool weightError = false;
		} analysis;


	public:

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl);

		void setParams(Params _params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;


		bool isValid() const override {
			return valid;
		}
		array_view<float> getData(layer_t layer) const override {
			return resultValues;
		}
		layer_t getLayersCount() const override {
			return 1;
		}
		layer_t getStartingLayer() const override {
			return analysis.minCascadeUsed;
		}

		const wchar_t* getProp(const isview& prop) const override;

	private:
		void updateValues(const SoundHandler& source, const BandResampler& resampler);
		void computeAnalysis(const BandResampler& resampler, layer_t startCascade, layer_t endCascade);
	};
}
