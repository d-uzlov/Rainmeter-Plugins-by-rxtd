// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include <array>
#include "AbstractFilter.h"
#include "rxtd/std_fixes/MyMath.h"

namespace rxtd::filter_utils {
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
		double gainAmp{};

	public:
		InfiniteResponseFilter() = default;
		InfiniteResponseFilter(std::vector<double> a, std::vector<double> b, double gainAmp);

		InfiniteResponseFilter(FilterParameters params) : InfiniteResponseFilter(
			std::move(params.a), std::move(params.b), params.gainAmp
		) { }

		// updates the state of the filter and returns filtered value
		double next(double value) {
			return updateState(value) * gainAmp;
		}

		void apply(array_span<float> signal) override {
			for (float& value : signal) {
				value = static_cast<float>(next(value));
			}
		}

		void reset() {
			std::fill(state.begin(), state.end(), 0.0);
		}

		void addGainDbEnergy(double gainDB) override {
			gainAmp *= std_fixes::MyMath::db2amplitude(gainDB * 0.5);
		}

	private:
		double updateState(double value);
	};

	template<index order>
	class InfiniteResponseFilterFixed : public AbstractFilter {
		// inspired by https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html

		std::array<double, order> a{};
		std::array<double, order> b{};
		std::array<double, order - 1> state{};
		double gainAmp{};

	public:
		InfiniteResponseFilterFixed() = default;

		InfiniteResponseFilterFixed(FilterParameters params) : InfiniteResponseFilterFixed(
			std::move(params.a), std::move(params.b), params.gainAmp
		) { }

		InfiniteResponseFilterFixed(array_view<double> _a, array_view<double> _b, double gainAmp) {
			if (_a.size() > order || _b.size() > order) {
				throw std::exception{};
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

		// updates the state of the filter and returns filtered value
		double next(double value) {
			// no [[nodiscard]] on the function because it may be used for state updating
			return updateState(value) * gainAmp;
		}

		void apply(array_span<float> signal) override {
			for (float& value : signal) {
				value = static_cast<float>(next(value));
			}
		}

		void reset() {
			std::fill(state.begin(), state.end(), 0.0);
		}

		void addGainDbEnergy(double gainDB) override {
			gainAmp *= std_fixes::MyMath::db2amplitude(gainDB * 0.5);
		}

	private:
		double updateState(const double value) {
			const double filtered = b[0] * value + state[0];

			const auto lastIndex = state.size() - 1;
			for (index i = 0; i < static_cast<index>(lastIndex); ++i) {
				auto i2 = static_cast<std::vector<float>::size_type>(i);
				const double ai = a[i2 + 1];
				const double bi = b[i2 + 1];
				const double prevState = state[i2 + 1];
				state[i2] = bi * value - ai * filtered + prevState;
			}

			state[lastIndex] = b[lastIndex + 1] * value - a[lastIndex + 1] * filtered;

			return filtered;
		}
	};
}
