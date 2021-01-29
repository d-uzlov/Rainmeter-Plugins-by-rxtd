/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::common::winapi_wrappers {
	template<typename T>
	class GenericCoTaskMemWrapper : MovableOnlyBase {
	public:
		using ObjectType = T;

	private:
		ObjectType* ptr = nullptr;

	public:
		GenericCoTaskMemWrapper() = default;

		template<typename InitFunction>
		GenericCoTaskMemWrapper(InitFunction initFunction) {
			const bool success = initFunction(&ptr);
			if (!success) {
				release();
			}
		}

		GenericCoTaskMemWrapper(GenericCoTaskMemWrapper&& other) noexcept {
			ptr = other.ptr;
			other.ptr = nullptr;
		}

		GenericCoTaskMemWrapper& operator=(GenericCoTaskMemWrapper&& other) noexcept {
			if (this == &other) {
				return *this;
			}

			release();

			ptr = other.ptr;
			other.ptr = nullptr;

			return *this;
		};

		virtual ~GenericCoTaskMemWrapper() {
			release();
		}

		void release() {
			if (ptr != nullptr) {
				CoTaskMemFree(ptr);
				ptr = nullptr;
			}
		}

		[[nodiscard]]
		bool isValid() const {
			return ptr != nullptr;
		}

		[[nodiscard]]
		ObjectType* getPointer() {
			return ptr;
		}
	};
}
