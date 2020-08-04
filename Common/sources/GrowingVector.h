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
		index size = 0;
		index offset = 0;

	public:
		[[nodiscard]]
		index getRemainingSize() const {
			return size - offset;
		}

		array_span<T> allocateNext(index chunkSize) {
			const index newSize = size + chunkSize;
			if (index(array.size()) < newSize) {
				array.resize(newSize);
			}
			array_span<T> result{ array.data() + size, chunkSize };
			size = newSize;
			return result;
		}

		array_span<T> takeChunk(index chunkSize) {
			if (chunkSize > getRemainingSize()) {
				return { };
			}
			array_span<T> result{ array.data() + offset, chunkSize };
			offset += chunkSize;
			return result;
		}

		[[nodiscard]]
		array_span<T> getFromTheEnd(index chunkSize) {
			return { array.data() + size - chunkSize, chunkSize };
		}

		[[nodiscard]]
		array_view<T> getFromTheEnd(index chunkSize) const {
			return { array.data() + size - chunkSize, chunkSize };
		}

		[[nodiscard]]
		array_span<T> getData() {
			return { array.data(), size };
		}

		[[nodiscard]]
		array_view<T> getData() const {
			return { array.data(), size };
		}

		void compact() {
			array.erase(array.begin(), array.begin() + offset);
			size = size - offset;
			offset = 0;
		}

		void reset() {
			size = 0;
			offset = 0;
		}
	};
}
