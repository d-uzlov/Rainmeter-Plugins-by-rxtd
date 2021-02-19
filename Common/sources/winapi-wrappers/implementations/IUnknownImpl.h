/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <atomic>

 // my-windows must be before any WINAPI include
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Unknwn.h>

namespace rxtd::common::winapi_wrappers {
	//
	// Base class for classes that implement some COM interface.
	//
	// IUnknown must free its resources on the last call to #Release().
	// The classic solution is to call "delete this;".
	// Derived classes must make sure that they are created with operator new.
	//
	template<typename T>
	class IUnknownImpl : NonMovableBase, virtual public T {
		static_assert(std::is_base_of<IUnknown, T>::value, "T must extend IUnknown");
		std::atomic_int referenceCounter{ 1 };

	protected:
		// can't use VirtualDestructorBase because destructor must be protected
		virtual ~IUnknownImpl() = default;

	public:
		ULONG STDMETHODCALLTYPE AddRef() final override {
			const auto prevValue = referenceCounter.fetch_add(1);
			return prevValue + 1;
		}

		ULONG STDMETHODCALLTYPE Release() final override {
			const auto newValue = referenceCounter.fetch_sub(1) - 1;
			if (newValue == 0) {
				delete this;
			}
			return newValue;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, void** ppvInterface) override {
			if (__uuidof(IUnknown) == riid) {
				AddRef();
				*ppvInterface = this;
			} else if (__uuidof(T) == riid) {
				AddRef();
				*ppvInterface = this;
			} else {
				*ppvInterface = nullptr;
				return E_NOINTERFACE;
			}
			return S_OK;
		}
	};
}
