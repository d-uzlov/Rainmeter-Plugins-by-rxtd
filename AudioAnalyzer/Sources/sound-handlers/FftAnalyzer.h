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

namespace rxtd::audio_analyzer {
	class FftAnalyzer : public SoundHandler {
	public:
		enum class SizeBy {
			BIN_WIDTH,
			SIZE,
			SIZE_EXACT
		};

		struct Params {
		private:
			friend FftAnalyzer;

			SizeBy sizeBy;

			double attackTime;
			double decayTime;

			double resolution;
			double overlap;

			layer_t cascadesCount;

			double randomTest;
			double randomDuration;
			bool correctZero;
		};

	private:
		struct CascadeData {
			FftAnalyzer *parent { };
			CascadeData *successor { };

			double attackDecay[2] { 0.0, 0.0 };
			std::vector<float> ringBuffer;
			std::vector<float> values;
			index filledElements { };
			index transferredElements { };
			float odd = 10.0f; // 10.0 means no value because valid values are in [-1.0; 1.0]
			double dc { };
			// Fourier transform looses energy due to downsample, so we multiply result of FFT by (2^0.5)^countOfDownsampleIterations
			double downsampleGain { };

			void setParams(FftAnalyzer* parent, CascadeData* successor, layer_t ind);
			void process(const float *wave, index waveSize);
			void processRandom(index waveSize, double amplitude);
			void processSilence(index waveSize);
			void reset();

		private:
			void doFft();
			void processResampled(const float* wave, index waveSize);
		};

		Params params { };

		index samplesPerSec { };

		index fftSize = 0;
		index inputStride = 0;

		index randomBlockSize = 0;
		index randomCurrentOffset = 0;
		enum class RandomState { ON, OFF } randomState { RandomState::ON };

		std::vector<CascadeData> cascades;

		FftImpl *fftImpl = nullptr;

		mutable string propString { };

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

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override { }

		bool isValid() const override {
			return true; // TODO
		}
		layer_t getLayersCount() const override;
		array_view<float> getData(layer_t layer) const override;

		const wchar_t* getProp(const isview& prop) const override;

		void setParams(Params params);

	private:
		void processRandom(index waveSize);
		void updateParams();
	};
}
