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
	template <typename T>
	class GrowingVector {
		std::vector<T> array{ };
		index offset = 0;
		index maxSize = 0;

	public:
		// Set the size after which buffer will be automatically compacted
		// Setting this value to 0 disables this behaviour
		// Default value is 0
		void setMaxSize(index value) {
			maxSize = value;
			array.reserve(maxSize);
		}

		[[nodiscard]]
		index getRemainingSize() const {
			return index(array.size()) - offset;
		}


		void pushNext(T value) {
			array.push_back(std::move(value));
		}

		[[nodiscard]]
		array_span<T> allocateNext(index chunkSize) {
			if (maxSize != 0 && index(array.size()) + chunkSize > maxSize) {
				compact();
			}

			array.resize(index(array.size()) + chunkSize);

			return getLast(chunkSize);
		}


		[[nodiscard]]
		array_span<T> getFirst(index chunkSize) {
			if (chunkSize > getRemainingSize()) {
				return { };
			}
			array_span<T> result{ array.data() + offset, chunkSize };
			return result;
		}

		[[nodiscard]]
		array_view<T> getFirst(index chunkSize) const {
			if (chunkSize > getRemainingSize()) {
				return { };
			}
			array_span<T> result{ array.data() + offset, chunkSize };
			return result;
		}


		[[nodiscard]]
		T* getPointer() {
			return array.data() + offset;
		}

		[[nodiscard]]
		const T* getPointer() const {
			return array.data() + offset;
		}


		array_span<T> removeFirst(index chunkSize) {
			if (chunkSize > getRemainingSize()) {
				return { };
			}
			array_span<T> result{ array.data() + offset, chunkSize };
			offset += chunkSize;
			return result;
		}


		[[nodiscard]]
		array_span<T> getLast(index chunkSize) {
			return { array.data() + index(array.size()) - chunkSize, chunkSize };
		}

		[[nodiscard]]
		array_view<T> getLast(index chunkSize) const {
			return { array.data() + index(array.size()) - chunkSize, chunkSize };
		}


		[[nodiscard]]
		array_span<T> getAllData() {
			return array;
		}

		[[nodiscard]]
		array_view<T> getAllData() const {
			return array;
		}


		// functionally does nothing
		void compact() {
			array.erase(array.begin(), array.begin() + offset);
			offset = 0;
		}

		// clear the vector
		void reset() {
			array.clear();
			offset = 0;
		}

		// resize vector to #count and fill it with #value
		void reset(index count, T value) {
			array.resize(count);
			std::fill_n(array.begin(), count, value);
			offset = 0;
		}
	};
}
