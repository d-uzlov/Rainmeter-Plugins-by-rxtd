/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Vector2D.h"

namespace rxtd::utils {
	template <typename T>
	class array2d_span {
		T* ptr { };
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		array2d_span() = default;
		array2d_span(T* ptr, index buffersCount, index bufferSize) :
			ptr(ptr),
			buffersCount(buffersCount),
			bufferSize(bufferSize) {
		}
		array2d_span(Vector2D<T>& source) :
			ptr(source[0].data()),
			buffersCount(source.getBuffersCount()),
			bufferSize(source.getBufferSize()) {
		}
		~array2d_span() = default;

		array2d_span(const array2d_span& other) = default;
		array2d_span(array2d_span&& other) noexcept = default;
		array2d_span& operator=(const array2d_span& other) = default;
		array2d_span& operator=(array2d_span&& other) noexcept = default;

		void fill(const T value = {}) {
			std::fill_n(ptr, buffersCount * bufferSize, value);
		}

		[[nodiscard]]
		constexpr index getBuffersCount() const {
			return buffersCount;
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
			return { ptr + bufferSize * bufferNumber, bufferSize };
		}
		[[nodiscard]]
		constexpr array_view<T> operator[](index bufferNumber) const {
			return { ptr + bufferSize * bufferNumber, bufferSize };
		}

		[[nodiscard]]
		constexpr array_span<T> getFlat() {
			return { ptr, buffersCount * bufferSize };
		}
		[[nodiscard]]
		constexpr array_view<T> getFlat() const {
			return { ptr, buffersCount * bufferSize };
		}
	};

	template <typename T>
	class array2d_view {
		const T* ptr { };
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		array2d_view() = default;
		array2d_view(const T* ptr, index buffersCount, index bufferSize) :
			ptr(ptr),
			buffersCount(buffersCount),
			bufferSize(bufferSize) {
		}
		array2d_view(const Vector2D<T>& source) :
			ptr(source[0].data()),
			buffersCount(source.getBuffersCount()),
			bufferSize(source.getBufferSize()) {
		}
		~array2d_view() = default;

		array2d_view(const array2d_view& other) = default;
		array2d_view(array2d_view&& other) noexcept = default;
		array2d_view& operator=(const array2d_view& other) = default;
		array2d_view& operator=(array2d_view&& other) noexcept = default;

		void fill(const T value = {}) {
			std::fill_n(ptr, buffersCount * bufferSize, value);
		}

		[[nodiscard]]
		constexpr index getBuffersCount() const {
			return buffersCount;
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
		constexpr array_view<T> operator[](index bufferNumber) const {
			return { ptr + bufferSize * bufferNumber, bufferSize };
		}

		[[nodiscard]]
		constexpr array_view<T> getFlat() const {
			return { ptr, buffersCount * bufferSize };
		}
	};
}
