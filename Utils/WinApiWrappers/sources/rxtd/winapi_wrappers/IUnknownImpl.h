// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

#include <atomic>

// my-windows must be before any WINAPI include
#include "rxtd/my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Unknwn.h>

namespace rxtd::winapi_wrappers {
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
			return static_cast<ULONG>(prevValue) + 1;
		}

		ULONG STDMETHODCALLTYPE Release() final override {
			const auto newValue = referenceCounter.fetch_sub(1) - 1;
			if (newValue == 0) {
				delete this;
			}
			return static_cast<ULONG>(newValue);
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, void** ppvInterface) override {
			if (__uuidof(IUnknown) == riid || __uuidof(T) == riid) {
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
