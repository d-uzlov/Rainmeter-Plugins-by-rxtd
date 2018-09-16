#pragma once
#include "DynamicArray.h"
#include <utility>

namespace pmrcbh {
	template <typename T>
	class ContinuousBuffersHolder {
		pmrda::DynamicArray<char> array;
		size_t buffersCount = 0;
		size_t bufferSize = 0;
		bool empty = true;

	public:
		ContinuousBuffersHolder() = default;
		~ContinuousBuffersHolder() = default;

		ContinuousBuffersHolder(const ContinuousBuffersHolder& other) = delete;
		ContinuousBuffersHolder(ContinuousBuffersHolder&& other) noexcept
			: array(std::move(other.array)),
			buffersCount(other.buffersCount),
			bufferSize(other.bufferSize),
			empty(other.empty) {
			other.buffersCount = 0;
			other.bufferSize = 0;
			other.empty = true;
		}
		ContinuousBuffersHolder& operator=(const ContinuousBuffersHolder& other) = delete;
		ContinuousBuffersHolder& operator=(ContinuousBuffersHolder&& other) noexcept {
			if (this == &other)
				return *this;
			array = std::move(other.array);

			buffersCount = other.buffersCount;
			other.buffersCount = 0;

			bufferSize = other.bufferSize;
			other.bufferSize = 0;

			empty = other.empty;
			other.empty = true;

			return *this;
		}
		void erase() {
			array.erase();
			bufferSize = 0;
			empty = true;
		}
		bool setBuffersCount(size_t count) {
			buffersCount = count;
			return setBufferSize(bufferSize);
		}
		bool setBufferSize(size_t size) {
			bufferSize = size;
			const auto newSize = buffersCount * size;
			if (newSize == 0) {
				return true;
			}
			empty = false;
			return array.ensureSize(newSize);
		}
		size_t getBufferSize() const {
			return bufferSize;
		}
		bool isEmpty() const {
			return empty;
		}
		T* getBuffer(unsigned int bufferNumber) {
			return reinterpret_cast<T*>(array.getPointer() + bufferSize * bufferNumber);
		}
		const T* getBuffer(unsigned int bufferNumber) const {
			return reinterpret_cast<const T*>(array.getPointer() + bufferSize * bufferNumber);
		}
		T* operator[](unsigned int bufferNumber) {
			return reinterpret_cast<T*>(array.getPointer() + bufferSize * bufferNumber);
		}
		const T* operator[](unsigned int bufferNumber) const {
			return reinterpret_cast<const T*>(array.getPointer() + bufferSize * bufferNumber);
		}
	};
}
