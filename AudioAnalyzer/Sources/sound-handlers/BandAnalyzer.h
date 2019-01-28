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

namespace rxaa {
	class BandAnalyzer : public SoundHandler {
	public:
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

			index minCascade { };
			index maxCascade { };
			istring fftId;
			std::vector<double> bandFreqs;
			double targetWeight { };
			double minWeight { };
			bool includeZero = true;
			bool proportionalValues = false;
			bool blurCascades = true;
			MixFunction minFunction { };
			double blurRadius = true;
			double offset { };
			double sensitivity { };
			SmoothingCurve smoothingCurve { };
			index smoothingFactor { };
			double exponentialFactor { };
		};
	private:
		struct BandValueInfo {
			double magnitude { };
			double weight { };
			double blurSigma { };

			void reset() {
				magnitude = 0.0;
				weight = 0.0;
				// blurRadius should not be changed
			}
		};

		class GaussianCoefficientsManager {
			// radius -> coefs vector
			std::unordered_map<index, std::vector<double>> gaussianBlurCoefficients;

		public:
			const std::vector<double>& forSigma(double sigma);

		private:
			static std::vector<double> generateGaussianKernel(index radius, double sigma);
		};
		mutable GaussianCoefficientsManager gcm;

		Params params { };

		index samplesPerSec { };

		std::vector<double> bandFreqMultipliers;
		double logNormalization { };

		mutable std::vector<std::vector<BandValueInfo>> bandInfo;
		mutable std::vector<std::vector<double>> pastValues;
		mutable index pastValuesIndex = 0;
		mutable std::vector<double> values;
		mutable std::vector<double> cascadeTempBuffer;

		mutable bool next = true;
		mutable bool analysisComputed = false;
		const FftAnalyzer* source = nullptr;

		string propString { };
		mutable struct {
			string analysisString { };
			index minCascadeUsed = -1;
			index maxCascadeUsed = -1;
		} analysis;


	public:

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl, utils::Rainmeter& rain);

		void setParams(Params params);

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		const double* getData() const override; // TODO vector_view?
		index getCount() const override;
		void setSamplesPerSec(index samplesPerSec) override;
		const wchar_t* getProp(const isview& prop) override;
		void reset() override;

	private:
		void updateValues() const;
		void computeAnalysis(index startCascade, index endCascade) const;

		static std::optional<std::vector<double>> parseFreqList(const utils::OptionParser::OptionList& bounds, utils::Rainmeter::ContextLogger& cl, const utils::Rainmeter& rain);
	};
}
