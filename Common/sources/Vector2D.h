/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"

namespace rxtd::utils {
	/**
	 * Class is used to use one array for several arrays of known size.
	 * More cache friendly than vector<vector<>>.
	 * Continuous, unlike vector<vector<>>.
	 */
	template <typename T>
	class Vector2D {
		std::vector<T> array{ };
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		void fill(const T value = { }) {
			std::fill_n(array.data(), buffersCount * bufferSize, value);
		}

		void setBuffersCount(index count) {
			buffersCount = count;
			setBufferSize(bufferSize);
		}

		[[nodiscard]]
		constexpr index getBuffersCount() const {
			return buffersCount;
		}

		void setBufferSize(index size) {
			bufferSize = size;
			array.resize(buffersCount * size);
		}

		[[nodiscard]]
		constexpr index getBufferSize() const {
			return bufferSize;
		}

		[[nodiscard]]
		constexpr bool isEmpty() const {
			return bufferSize == 0;
		}

		[[nodiscard]]
		constexpr array_span<T> operator[](index bufferNumber) {
			return { array.data() + bufferSize * bufferNumber, bufferSize };
		}
		
		[[nodiscard]]
		constexpr array_view<T> operator[](index bufferNumber) const {
			return { array.data() + bufferSize * bufferNumber, bufferSize };
		}

		[[nodiscard]]
		constexpr array_span<T> getFlat() {
			return { array.data(), buffersCount * bufferSize };
		}

		[[nodiscard]]
		constexpr array_view<T> getFlat() const {
			return { array.data(), buffersCount * bufferSize };
		}
	};
}
