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
#include <chrono>

namespace rxaa {
	class BandAnalyzer : public SoundHandler {
	public:
		using cascade_t = FftAnalyzer::cascade_t;
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		enum class SmoothingCurve {
			FLAT,
			LINEAR,
			EXPONENTIAL,
		};
		enum class MixFunction {
			AVERAGE,
			PRODUCT,
		};
		struct Params {
		private:
			friend BandAnalyzer;

			istring fftId;
			cascade_t minCascade;
			cascade_t maxCascade;

			std::vector<double> bandFreqs;

			double minWeight;
			double targetWeight;
			double weightFallback;

			double zeroLevel;
			double zeroLevelHard;
			double zeroWeight;

			bool includeDC;
			bool proportionalValues;

			MixFunction mixFunction;

			double blurRadiusMultiplier;

			index minBlurRadius;
			index maxBlurRadius;
			double blurMinAdaptation;
			double blurMaxAdaptation;

			double offset;
			double sensitivity;

			SmoothingCurve smoothingCurve;
			index smoothingFactor;
			double exponentialFactor;
		};

	private:
		struct CascadeInfo {
			struct BandInfo {
				double weight { };
				double blurSigma { };
				cascade_t endCascade { };
			};

			std::vector<double> magnitudes;
			std::vector<BandInfo> bandsInfo;

			void setSize(index size) {
				magnitudes.resize(size);
				bandsInfo.resize(size);
			}
		};

		class GaussianCoefficientsManager {
			// radius -> coefs vector
			std::unordered_map<index, std::vector<double>> blurCoefficients;

			index minRadius { };
			index maxRadius { };

		public:
			const std::vector<double>& forSigma(double sigma);
			void setRadiusBounds(index min, index max);

		private:
			static std::vector<double> generateGaussianKernel(index radius, double sigma);
		};
		GaussianCoefficientsManager gcm;

		Params params { };

		index samplesPerSec { };

		std::vector<double> bandFreqMultipliers;
		mutable index bandsCount = 0;
		double logNormalization { };

		std::vector<CascadeInfo> cascadesInfo;
		std::vector<std::vector<double>> pastValues;
		cascade_t pastValuesIndex = 0;

		std::vector<double> values;
		std::vector<double> cascadeTempBuffer;

		bool changed = true;
		bool analysisComputed = false;
		const FftAnalyzer* source = nullptr;
		clock::time_point lastFilteringTime { };

		mutable string propString { };

		struct {
			string analysisString { };
			cascade_t minCascadeUsed = -1;
			cascade_t maxCascadeUsed = -1;
			std::vector<cascade_t> bandEndCascades;
			bool weightError = false;
		} analysis;


	public:

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl, utils::Rainmeter& rain);

		void setParams(Params _params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		const double* getData() const override; // TODO vector_view?
		index getCount() const override;

		const wchar_t* getProp(const isview& prop) const override;

	private:
		void updateValues();
		void computeBandInfo(cascade_t startCascade, cascade_t endCascade);
		void computeAnalysis(cascade_t startCascade, cascade_t endCascade);
		void sampleData();
		void blurData();
		void gatherData();
		void applyTimeFiltering();
		void transformToLog();

		static std::optional<std::vector<double>> parseFreqList(const utils::OptionParser::OptionList& bounds, utils::Rainmeter::ContextLogger& cl, const utils::Rainmeter& rain);
	};
}
