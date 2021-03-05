// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

#include <Combaseapi.h>

namespace rxtd::winapi_wrappers {
	//
	// Wrapper over objects that need to be freed with CoTaskMemFree.
	// Exists for RAII
	//
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
		}

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
