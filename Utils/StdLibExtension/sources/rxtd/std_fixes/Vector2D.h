/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::std_fixes {
	template<typename T>
	class array2d_span;
	template<typename T>
	class array2d_view;

	/**
	 * Class is used to use one array for several arrays of known size.
	 * More cache friendly than vector<vector<>>.
	 * Continuous, unlike vector<vector<>>.
	 */
	template<typename T>
	class Vector2D {
		std::vector<T> array{};
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		void fill(const T value = {}) {
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

		constexpr void transferToSpan(array2d_span<T> dest) const;

		constexpr void transferToVector(Vector2D<T>& dest) const {
			dest.setBuffersCount(buffersCount);
			dest.setBufferSize(bufferSize);
			getFlat().transferToSpan(dest.getFlat());
		}

		constexpr void copyFrom(array2d_view<T> source);

		constexpr void copyWithResize(array2d_view<T> source);
	};


	template<typename T>
	class array2d_span {
		T* ptr{};
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		array2d_span() = default;

		array2d_span(T* ptr, index buffersCount, index bufferSize) :
			ptr(ptr),
			buffersCount(buffersCount),
			bufferSize(bufferSize) { }

		array2d_span(Vector2D<T>& source) :
			ptr(source[0].data()),
			buffersCount(source.getBuffersCount()),
			bufferSize(source.getBufferSize()) { }

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

		constexpr void transferToSpan(array2d_span<T> dest) const {
			if (dest.buffersCount != buffersCount || dest.bufferSize != bufferSize)
				throw std::out_of_range("array2d_view::transferToSpan");
			getFlat().transferToSpan(dest.getFlat());
		}

		constexpr void transferToVector(Vector2D<T>& dest) const {
			dest.setBuffersCount(buffersCount);
			dest.setBufferSize(bufferSize);
			getFlat().transferToSpan(dest.getFlat());
		}

		constexpr void copyFrom(array2d_view<T> source);
	};

	template<typename T>
	class array2d_view {
		const T* ptr{};
		index buffersCount = 0;
		index bufferSize = 0;

	public:
		array2d_view() = default;

		array2d_view(const T* ptr, index buffersCount, index bufferSize) :
			ptr(ptr),
			buffersCount(buffersCount),
			bufferSize(bufferSize) { }

		array2d_view(const Vector2D<T>& source) :
			ptr(source[0].data()),
			buffersCount(source.getBuffersCount()),
			bufferSize(source.getBufferSize()) { }

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

		constexpr void transferToSpan(array2d_span<T> dest) const {
			if (dest.getBuffersCount() != buffersCount || dest.getBufferSize() != bufferSize)
				throw std::out_of_range("array2d_view::transferToSpan");
			getFlat().transferToSpan(dest.getFlat());
		}

		constexpr void transferToVector(Vector2D<T>& dest) const {
			dest.setBuffersCount(buffersCount);
			dest.setBufferSize(bufferSize);
			getFlat().transferToSpan(dest.getFlat());
		}
	};


	template<typename T>
	constexpr void array2d_span<T>::copyFrom(array2d_view<T> source) {
		source.transferToSpan(*this);
	}

	template<typename T>
	constexpr void Vector2D<T>::transferToSpan(array2d_span<T> dest) const {
		if (dest.getBuffersCount() != buffersCount || dest.getBufferSize() != bufferSize)
			throw std::out_of_range("Vector2D::transferToSpan");
		getFlat().transferToSpan(dest.getFlat());
	}

	template<typename T>
	constexpr void Vector2D<T>::copyFrom(array2d_view<T> source) {
		source.transferToChecked(*this);
	}

	template<typename T>
	constexpr void Vector2D<T>::copyWithResize(array2d_view<T> source) {
		setBuffersCount(source.getBuffersCount());
		setBufferSize(source.getBufferSize());
		source.transferToSpan(*this);
	}
}
