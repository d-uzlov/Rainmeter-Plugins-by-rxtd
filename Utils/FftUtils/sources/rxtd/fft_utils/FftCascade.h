// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include <chrono>
#include <functional>

#include "FFT.h"
#include "rxtd/GrowingVector.h"
#include "rxtd/filter_utils/DownsampleHelper.h"

namespace rxtd::fft_utils {
	class FftCascade {
	public:
		using DownsampleHelper = filter_utils::DownsampleHelper;
		
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		struct Params {
			index fftSize;
			index samplesPerSec;

			index inputStride;

			std::function<void(array_view<float> result, index cascade)> callback;
		};

	private:
		FftCascade* successorPtr{};
		FFT* fftPtr{};

		Params params{};
		index cascadeIndex{};

		GrowingVector<float> buffer;
		DownsampleHelper downsampleHelper{ 2 };
		std::vector<float> values;
		bool hasChanges = false;

	public:
		void setParams(Params _params, FFT* _fftPtr, FftCascade* _successorPtr, index _cascadeIndex);
		void process(array_view<float> wave, clock::time_point killTime);

	private:
		void resampleResult();
		void doFft(array_view<float> chunk);
	};
}
