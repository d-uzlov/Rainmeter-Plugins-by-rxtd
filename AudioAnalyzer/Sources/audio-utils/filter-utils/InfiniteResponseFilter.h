/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"
#include <array>
#include "AbstractFilter.h"

namespace rxtd::audio_utils {
	struct FilterParameters {
		std::vector<double> a;
		std::vector<double> b;
		double gainAmp;
	};

	class InfiniteResponseFilter : public AbstractFilter {
		// inspired by https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html

		std::vector<double> a;
		std::vector<double> b;
		std::vector<double> state;
		double gainAmp{ };

	public:
		InfiniteResponseFilter() = default;
		InfiniteResponseFilter(std::vector<double> a, std::vector<double> b, double gainAmp);

		InfiniteResponseFilter(FilterParameters params) : InfiniteResponseFilter(
			std::move(params.a), std::move(params.b), params.gainAmp) {
		}

		void apply(array_span<float> signal) override;

	private:
		void updateState(double next, double nextFiltered);
	};

	template <index order>
	class InfiniteResponseFilterFixed : public AbstractFilter {
		// inspired by https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html

		std::array<double, order> a{ };
		std::array<double, order> b{ };
		std::array<double, order - 1> state{ };
		double gainAmp{ };

	public:
		InfiniteResponseFilterFixed() = default;

		InfiniteResponseFilterFixed(FilterParameters params) : InfiniteResponseFilterFixed(
			std::move(params.a), std::move(params.b), params.gainAmp) {
		}

		InfiniteResponseFilterFixed(std::vector<double> _a, std::vector<double> _b, double gainAmp) {
			if (_a.size() > order || _b.size() > order) {
				throw std::exception{ };
			}

			this->gainAmp = gainAmp;

			std::copy(_a.begin(), _a.end(), a.begin());
			std::copy(_b.begin(), _b.end(), b.begin());

			const double a0 = a[0];
			for (auto& value : a) {
				value /= a0;
			}
			for (auto& value : b) {
				value /= a0;
			}
		}

		void apply(array_span<float> signal) override {
			for (float& value : signal) {
				const double next = value;
				const double nextFiltered = b[0] * next + state[0];
				value = float(nextFiltered * gainAmp);
				updateState(next, nextFiltered);
			}
		}

	private:
		void updateState(double next, double nextFiltered) {
			const index lastIndex = state.size() - 1;
			for (index i = 0; i < lastIndex; ++i) {
				const double ai = a[i + 1];
				const double bi = b[i + 1];
				const double prevD = state[i + 1];
				state[i] = bi * next - ai * nextFiltered + prevD;
			}
			state[lastIndex] = b[lastIndex + 1] * next - a[lastIndex + 1] * nextFiltered;
		}
	};
}
