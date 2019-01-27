/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	/**
	 * Class is used to use one array for several arrays of known size.
	 * More cache friendly than vector<vector<>>.
	 * Continuous, unlike vector<vector<>>.
	 */
	template <typename T>
	class Vector2D {
		std::vector<T> array { };
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		Vector2D() = default;
		~Vector2D() = default;

		Vector2D(Vector2D&& other) noexcept :
			array(std::move(other.array)),
			buffersCount(other.buffersCount),
			bufferSize(other.bufferSize) {
			other.buffersCount = 0;
			other.bufferSize = 0;
		}
		Vector2D& operator=(Vector2D&& other) noexcept {
			if (this == &other)
				return *this;

			array = std::move(other.array);

			buffersCount = other.buffersCount;
			other.buffersCount = 0;

			bufferSize = other.bufferSize;
			other.bufferSize = 0;

			return *this;
		}

		Vector2D(const Vector2D& other) = delete;
		Vector2D& operator=(const Vector2D& other) = delete;

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
