/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <type_traits>
#include <Unknwn.h>

namespace rxtd::utils {
	template <typename T>
	class GenericComWrapper {
		static_assert(std::is_base_of<IUnknown, T>::value, "T must extend IUnknown");
		T *ptr = nullptr;

	public:

		GenericComWrapper() = default;

		GenericComWrapper(GenericComWrapper&& other) noexcept {
			release();

			ptr = other.ptr;
			other.ptr = nullptr;
		}
		GenericComWrapper& operator=(GenericComWrapper&& other) noexcept {
			release();

			ptr = other.ptr;
			other.ptr = nullptr;

			return *this;
		};

		GenericComWrapper(const GenericComWrapper& other) = delete;
		GenericComWrapper& operator=(const GenericComWrapper& other) = delete;

		~GenericComWrapper() {
			release();
		}

		void release() {
			if (ptr != nullptr) {
				ptr->Release();
				ptr = nullptr;
			}
		}

		bool isValid() const {
			return ptr != nullptr;
		}

		T* getPointer() {
			return ptr;
		}

		T** operator &() {
			return &ptr;
		}
		T* operator->() {
			return ptr;
		}
		const T* operator->() const {
			return ptr;
		}
	};
}
