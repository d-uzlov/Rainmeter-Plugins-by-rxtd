/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::audio_utils {
	class MinMaxCounter {
		index blockSize = 1;
		index counter{};
		float min{};
		float max{};
		array_view<float> wave;

	public:
		void setBlockSize(index value) {
			blockSize = value;
		}

		void setWave(array_view<float> value) {
			wave = value;
		}

		void update() {
			const index remainingBlockSize = std::min(blockSize - counter, wave.size());
			if (remainingBlockSize == 0) {
				return;
			}

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
		bool isEmpty() const {
			return wave.empty();
		}

		[[nodiscard]]
		bool isReady() const {
			return counter == blockSize;
		}

		[[nodiscard]]
		float getMin() const {
			return min;
		}

		[[nodiscard]]
		float getMax() const {
			return max;
		}

		void reset() {
			counter = 0;
			min = std::numeric_limits<float>::infinity();
			max = -std::numeric_limits<float>::infinity();
		}
	};
}
