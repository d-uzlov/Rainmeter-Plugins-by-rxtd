/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <Unknwn.h>

namespace rxtd::utils {
	template <typename T>
	class IUnknownImpl : virtual public T {
		static_assert(std::is_base_of<IUnknown, T>::value, "T must extend IUnknown");
		LONG _cRef = 1;

	public:
		IUnknownImpl() = default;
		virtual ~IUnknownImpl() = default;

		IUnknownImpl(const IUnknownImpl& other) = delete;
		IUnknownImpl(IUnknownImpl&& other) noexcept = delete;
		IUnknownImpl& operator=(const IUnknownImpl& other) = delete;
		IUnknownImpl& operator=(IUnknownImpl&& other) noexcept = delete;

		ULONG STDMETHODCALLTYPE AddRef() final override {
			return InterlockedIncrement(&_cRef);
		}

		ULONG STDMETHODCALLTYPE Release() final override {
			const ULONG ulRef = InterlockedDecrement(&_cRef);
			if (0 == ulRef) {
				delete this;
			}
			return ulRef;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvInterface) override {
			if (IID_IUnknown == riid) {
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
