// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::audio_analyzer::audio_utils {
	class MinMaxCounter {
		index blockSize = 1;
		index counter{};
		float min = std::numeric_limits<float>::infinity();
		float max = -std::numeric_limits<float>::infinity();
		array_view<float> wave;

	public:
		void setBlockSize(index value) {
			blockSize = value;
		}

		void setWave(array_view<float> value) {
			wave = value;
		}

		void update() {
			if (wave.empty() || counter >= blockSize) {
				return;
			}

			const index remainingBlockSize = std::min(blockSize - counter, wave.size());

			auto [wMin, wMax] = std::minmax_element(wave.begin(), wave.begin() + remainingBlockSize);
			min = std::min(min, *wMin);
			max = std::max(max, *wMax);

			wave.remove_prefix(remainingBlockSize);
			counter += remainingBlockSize;
		}

		[[nodiscard]]
		bool isBelowThreshold(float value) const {
			return std::abs(min) < value
				&& std::abs(max) < value;
		}

		[[nodiscard]]
		bool hasNext() const {
			const index missingBlockSize = blockSize - counter;
			return wave.size() >= missingBlockSize;
		}

		[[nodiscard]]
		float getMin() const {
			return min;
		}

		[[nodiscard]]
		float getMax() const {
			return max;
		}

		void skipBlock() {
			counter -= blockSize;
			min = std::numeric_limits<float>::infinity();
			max = -std::numeric_limits<float>::infinity();
		}
	};
}
