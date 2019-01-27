/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "FftImpl.h"
#include "SoundHandler.h"
#include <random>
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxaa {
	class FftAnalyzer : public SoundHandler {
	public:
		enum class SizeBy {
			RESOLUTION,
			SIZE,
			SIZE_EXACT
		};
		struct Params {
		private:
			friend FftAnalyzer;

			SizeBy sizeBy { };

			double attackTime { };
			double decayTime { };

			double resolution { };
			double overlap = 0.0;

			index cascadesCount = 0u;

			double randomTest = 0.0;
			bool correctZero = true;
		};

	private:
		struct CascadeData {
			FftAnalyzer *parent { };
			CascadeData *successor { };

			double attackDecay[2] { 0.0, 0.0 };
			std::vector<float> ringBuffer;
			std::vector<double> values;
			index filledElements { };
			index transferredElements { };
			float odd = 10.0f; // 10.0 means no value because valid values are in [-1.0; 1.0]
			double dc { };
			// Fourier transform looses energy due to downsample, so we multiply result of FFT by (2^0.5)^countOfDownsampleIterations
			double downsampleGain { };

			void setParams(FftAnalyzer* parent, CascadeData* successor, index index);
			void process(const float *wave, index waveSize);
			void processRandom(index waveSize, double amplitude);
			void processSilent(index waveSize);
			void reset();

		private:
			void doFft();
			void processResampled(const float* wave, index waveSize);
		};

		Params params { };

		index samplesPerSec { };

		index fftSize = 0u;

		index inputStride = 0u;

		std::vector<CascadeData> cascades;

		FftImpl *fftImpl = nullptr;

		string propString { };

		class Random {
			std::random_device rd;
			std::mt19937 e2;
			std::uniform_real_distribution<> dist;

		public:
			Random();
			double next();
		} random;

	public:
		FftAnalyzer() = default;

		~FftAnalyzer();
		/** This class is non copyable */
		FftAnalyzer(const FftAnalyzer& other) = delete;
		FftAnalyzer(FftAnalyzer&& other) = delete;
		FftAnalyzer& operator=(const FftAnalyzer& other) = delete;
		FftAnalyzer& operator=(FftAnalyzer&& other) = delete;

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl);

		double getFftFreq(index fft) const;

		index getFftSize() const;
		index getCascadesCount() const;
		const double* getCascade(index cascade) const;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		const double* getData() const override;
		index getCount() const override;
		void setSamplesPerSec(index samplesPerSec) override;
		const wchar_t* getProp(const isview& prop) override;
		void reset() override;

		void setParams(Params params);

	private:
		void updateParams();
	};
}
