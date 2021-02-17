/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <type_traits>

// my-windows must be before any WINAPI include
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Unknwn.h>

#include "ComException.h"

namespace rxtd::common::winapi_wrappers {
	//
	// Wrapper over IUnknown derived class
	// Mostly exists for RAII
	//
	template<typename T>
	class GenericComWrapper : VirtualDestructorBase {
	public:
		using ComException = ComException;
		using InitFunctionType = bool(T** ptr);
		using ObjectType = T;
		static_assert(std::is_base_of<IUnknown, T>::value, "T must extend IUnknown");

	private:
		ObjectType* ptr = nullptr;

	public:
		GenericComWrapper() = default;

		template<typename InitFunction>
		GenericComWrapper(InitFunction initFunction) {
			const bool success = initFunction(&ptr);
			if (!success) {
				release();
			}
		}

		GenericComWrapper(GenericComWrapper&& other) noexcept {
			ptr = other.ptr;
			other.ptr = nullptr;
		}

		GenericComWrapper& operator=(GenericComWrapper&& other) noexcept {
			if (this == &other) {
				return *this;
			}

			release();

			ptr = other.ptr;
			other.ptr = nullptr;

			return *this;
		}

		GenericComWrapper(const GenericComWrapper& other) {
			ptr = other.ptr;
			if (ptr != nullptr) {
				ptr->AddRef();
			}
		}

		GenericComWrapper& operator=(const GenericComWrapper& other) {
			if (this == &other) {
				return *this;
			}

			release();

			ptr = other.ptr;
			if (ptr != nullptr) {
				ptr->AddRef();
			}

			return *this;
		}

		virtual ~GenericComWrapper() {
			release();
		}

		void release() {
			if (ptr != nullptr) {
				ptr->Release();
				ptr = nullptr;
			}
		}

		[[nodiscard]]
		bool isValid() const {
			return ptr != nullptr;
		}

		[[nodiscard]]
		ObjectType& ref() {
			return *ptr;
		}

		template<typename Interface, typename MethodType, typename... Args>
		HRESULT typedQuery(MethodType method, Interface** interfacePtr, Args ... args) {
			return (ptr->*method)(__uuidof(Interface), args..., reinterpret_cast<void**>(interfacePtr));
		}

		static void throwOnError(HRESULT code, sview source) noexcept(false) {
			if (code != S_OK) {
				throw ComException{ code, source };
			}
		}
	};
}
