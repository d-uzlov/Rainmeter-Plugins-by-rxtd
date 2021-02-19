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

		/// <summary>
		/// InitFunction must be an object that have operator() overloaded to accept ObjectType** value.
		/// InitFunction must initialize ObjectType* value of the pointer it accepted or leave as nullptr.
		/// InitFunction is advised to throw an exception if it is unable to initialize the pointer.
		/// </summary>
		template<typename InitFunction>
		GenericComWrapper(InitFunction initFunction) noexcept(false) {
			initFunction(&ptr);
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
		bool isValid() const noexcept {
			return ptr != nullptr;
		}

		[[nodiscard]]
		ObjectType& ref() {
			return *ptr;
		}

		/// <summary>
		/// Calls method with __uuidof(Interface), specified args and a pointer on this object.
		/// Example: replace IMMDevice::Activate(CLSCTX_INPROC_SERVER, nullptr, &ptr) with typedQuery(&IMMDevice::Activate, ptr, CLSCTX_INPROC_SERVER, nullptr)
		/// Intended to be used as a type-safe replacement
		/// for type-unsafe calls to object creation functions in COM API.
		/// Throws ComException on error.
		/// </summary>
		/// <typeparam name="Interface">Class to take uuid of.</typeparam>
		/// <param name="method">Method to call.</param>
		/// <param name="errorMessage">Message for the exception.</param>
		/// <param name="interfacePtr">Pointer that will receive the value from method.</param>
		/// <param name="args">All the args for MethodType except uuid and last receiving pointer.</param>
		template<typename Interface, typename MethodType, typename... Args>
		void typedQuery(MethodType method, Interface** interfacePtr, sview errorMessage, Args ... args) noexcept(false) {
			throwOnError((ptr->*method)(__uuidof(Interface), args..., reinterpret_cast<void**>(interfacePtr)), errorMessage);
		}

		/// <summary>
		/// Compares code with S_OK value.
		/// If code != S_OK then throws ComException with provided error message.
		/// </summary>
		/// <param name="code">WASAPI return code to check.</param>
		/// <param name="source">Message for the exception.</param>
		static void throwOnError(HRESULT code, sview source) noexcept(false) {
			if (code != S_OK) {
				throw ComException{ code, source };
			}
		}
	};
}
