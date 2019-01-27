/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils { // TODO rewrite
	/**
	 * Class is used to use one array for several arrays of known size.
	 * Main purpose is to make it more cache friendly than vector<vector<T>>, I guess.
	 */
	template <typename T>
	class ContinuousBuffersHolder {
		std::vector<T> array { };
		index buffersCount = 0;
		index bufferSize = 0;

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

		void setBuffersCount(index count) {
			buffersCount = count;
			setBufferSize(bufferSize);
		}
		index getBuffersCount() const {
			return buffersCount;
		}

		void setBufferSize(index size) {
			bufferSize = size;
			array.reserve(buffersCount * size);
		}
		index getBufferSize() const {
			return bufferSize;
		}

		bool isEmpty() const {
			return bufferSize == 0;
		}

		T* operator[](index bufferNumber) {
			return array.data() + bufferSize * bufferNumber;
		}
		const T* operator[](index bufferNumber) const {
			return array.data() + bufferSize * bufferNumber;
		}
	};
}
