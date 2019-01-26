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
#include <vector>
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
			int minCascade { };
			int maxCascade { };
			std::wstring fftId;
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
			unsigned smoothingFactor { };
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
			std::unordered_map<unsigned, std::vector<double>> gaussianBlurCoefficients;

		public:
			const std::vector<double>& forSigma(double sigma);

		private:
			static std::vector<double> generateGaussianKernel(int radius, double sigma);
		};
		mutable GaussianCoefficientsManager gcm;

		Params params { };

		uint32_t samplesPerSec { };

		std::vector<double> bandFreqMultipliers;
		double logNormalization { };

		mutable std::vector<std::vector<BandValueInfo>> bandInfo;
		mutable std::vector<std::vector<double>> pastValues;
		mutable size_t pastValuesIndex = 0u;
		mutable std::vector<double> values;
		mutable std::vector<double> cascadeTempBuffer;

		mutable bool next = true;
		mutable bool analysisComputed = false;
		const FftAnalyzer* source = nullptr;

		std::wstring propString { };
		mutable struct {
			std::wstring analysisString { };
			int minCascadeUsed = -1;
			int maxCascadeUsed = -1;
		} analysis;


	public:

		static std::optional<Params> parseParams(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger& cl, rxu::Rainmeter& rain);

		void setParams(Params params);

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		const double* getData() const override;
		size_t getCount() const override;
		void setSamplesPerSec(uint32_t samplesPerSec) override;
		const wchar_t* getProp(const std::wstring_view& prop) override;
		void reset() override;

	private:
		void updateValues() const;
		void computeAnalysis(unsigned startCascade, unsigned endCascade) const;


		static std::optional<std::vector<double>> parseFreqList(rxu::OptionParser::OptionList bounds, rxu::Rainmeter::ContextLogger& cl, const rxu::Rainmeter& rain);
	};
}
