#pragma once
#include <cstdlib>

namespace pmrda {
	template <typename T> class DynamicArray {
	private:
		T* array = nullptr;
		size_t arraySize = 0;

	public:
		DynamicArray() = default;
		DynamicArray(const DynamicArray& other) = delete;
		DynamicArray(DynamicArray&& other) noexcept
			: array(other.array),
			  arraySize(other.arraySize) {
			other.array = nullptr;
			other.arraySize = 0;
		}
		DynamicArray& operator=(const DynamicArray& other) = delete;
		DynamicArray& operator=(DynamicArray&& other) noexcept {
			if (this == &other)
				return *this;
			free(array);
			array = other.array;
			other.array = nullptr;

			arraySize = other.arraySize;
			other.arraySize = 0;
			return *this;
		}
		~DynamicArray() noexcept {
			free(array);
			array = nullptr;
		}

		T* getPointer() noexcept {
			return array;
		}
		const T* getPointer() const noexcept {
			return array;
		}
		bool ensureElementsCount(size_t count) noexcept {
			return ensureSize(count * sizeof(T));
		}
		bool ensureSize(size_t size) noexcept {
			if (arraySize < size) {
				free(array);
				if (size > 2000) {
					const size_t pages = (size - 1) >> 12;
					size = (pages + 1) << 12;
				}
				array = static_cast<T*>(malloc(size));
				if (array == nullptr) {
					arraySize = 0;
					return false;
				}
				arraySize = size;
				return true;
			}
			return true;
		}
		void erase() noexcept {
			free(array);
			array = nullptr;
			arraySize = 0;
		}
		size_t getArraySize() const {
			return arraySize;
		}
		T& operator[] (size_t index) {
			return array[index];
		}
		const T& operator[] (size_t index) const {
			return array[index];
		}
	};
}
