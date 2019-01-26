/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <vector>

namespace rxu { // TODO rewrite
	/**
	 * Class is used to use one array for several arrays of known size.
	 * Main purpose is to make it more cache friendly than vector<vector<T>>, I guess.
	 */
	template <typename T>
	class ContinuousBuffersHolder {
		std::vector<T> array;
		size_t buffersCount = 0;
		size_t bufferSize = 0;

	public:
		ContinuousBuffersHolder() = default;
		~ContinuousBuffersHolder() = default;

		ContinuousBuffersHolder(ContinuousBuffersHolder&& other) noexcept :
			array(std::move(other.array)),
			buffersCount(other.buffersCount),
			bufferSize(other.bufferSize) {
			other.buffersCount = 0;
			other.bufferSize = 0;
		}
		ContinuousBuffersHolder& operator=(ContinuousBuffersHolder&& other) noexcept {
			if (this == &other)
				return *this;

			array = std::move(other.array);

			buffersCount = other.buffersCount;
			other.buffersCount = 0;

			bufferSize = other.bufferSize;
			other.bufferSize = 0;

			return *this;
		}

		ContinuousBuffersHolder(const ContinuousBuffersHolder& other) = delete;
		ContinuousBuffersHolder& operator=(const ContinuousBuffersHolder& other) = delete;

		void setBuffersCount(size_t count) {
			buffersCount = count;
			setBufferSize(bufferSize);
		}
		size_t getBuffersCount() const {
			return buffersCount;
		}

		void setBufferSize(size_t size) {
			bufferSize = size;
			array.reserve(buffersCount * size);
		}
		size_t getBufferSize() const {
			return bufferSize;
		}

		bool isEmpty() const {
			return bufferSize == 0;
		}

		T* operator[](size_t bufferNumber) {
			return array.data() + bufferSize * bufferNumber;
		}
		const T* operator[](size_t bufferNumber) const {
			return array.data() + bufferSize * bufferNumber;
		}
	};
}
