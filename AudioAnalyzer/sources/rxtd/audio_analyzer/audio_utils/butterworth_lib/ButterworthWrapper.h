/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/audio_analyzer/audio_utils/filter_utils/InfiniteResponseFilter.h"

namespace rxtd::audio_analyzer::audio_utils::butterworth_lib {
	class ButterworthWrapper {
		// https://stackoverflow.com/questions/10373184/bandpass-butterworth-filter-implementation-in-c
		// http://www.exstrom.com/journal/sigproc/

	public:
		using SizeFuncSignature = index(*)(index order);

		class GenericCoefCalculator {
			using CoefFuncSignature = double* (*)(int order, double f1, double f2);
			using ScalingFuncSignature = double (*)(int n, double f1, double f2);

			const CoefFuncSignature aFunc;
			const CoefFuncSignature bFunc;
			const ScalingFuncSignature sFunc;
			const SizeFuncSignature sizeFunc;

		public:
			using FilterParameters = filter_utils::FilterParameters;

			GenericCoefCalculator(
				CoefFuncSignature aFunc, CoefFuncSignature bFunc, ScalingFuncSignature sFunc, SizeFuncSignature sizeFunc
			) : aFunc(aFunc), bFunc(bFunc), sFunc(sFunc), sizeFunc(sizeFunc) { }

			[[nodiscard]]
			index filterSize(index order) const {
				return sizeFunc(order);
			}

			[[nodiscard]]
			FilterParameters calcCoefDigital(index order, double digitalCutoff) const {
				return calcCoefDigital(order, digitalCutoff, 0.0);
			}

			[[nodiscard]]
			FilterParameters calcCoef(index order, double samplingFrequency, double cutoffFrequency) const {
				return calcCoefDigital(order, 2.0 * cutoffFrequency / samplingFrequency);
			}

			[[nodiscard]]
			FilterParameters calcCoefDigital(index _order, double digitalCutoffLow, double digitalCutoffHigh) const {
				const int order = int(_order);
				if (order < 0) {
					return {};
				}

				digitalCutoffLow = std::clamp(digitalCutoffLow, 0.01, 1.0 - 0.01);
				digitalCutoffHigh = std::clamp(digitalCutoffHigh, 0.01, 1.0 - 0.01);

				const index size = filterSize(order);

				return {
					wrapCoefs(aFunc, size, order, digitalCutoffLow, digitalCutoffHigh),
					wrapCoefs(bFunc, size, order, digitalCutoffLow, digitalCutoffHigh),
					sFunc(order, digitalCutoffLow, digitalCutoffHigh)
				};
			}

			[[nodiscard]]
			FilterParameters calcCoef(
				index order,
				double samplingFrequency,
				double lowerCutoffFrequency, double upperCutoffFrequency
			) const {
				return calcCoefDigital(
					order,
					2.0 * lowerCutoffFrequency / samplingFrequency,
					2.0 * upperCutoffFrequency / samplingFrequency
				);
			}

		private:
			template<typename T, typename... Args>
			[[nodiscard]]
			static std::vector<double> wrapCoefs(T* (*funcPtr)(Args ...), index resultSize, Args ... args) {
				std::vector<double> result;

				T* coefs = funcPtr(args...);

				result.resize(resultSize);
				for (index i = 0; i < index(result.size()); ++i) {
					result[i] = coefs[i];
				}

				free(coefs);

				return result;
			}
		};

		static constexpr index oneSideSlopeSize(index order) {
			return order + 1;
		}

		static constexpr index twoSideSlopeSize(index order) {
			return 2 * order + 1;
		}

		static const GenericCoefCalculator lowPass;
		static const GenericCoefCalculator highPass;
		static const GenericCoefCalculator bandPass;
		static const GenericCoefCalculator bandStop;
	};
}
