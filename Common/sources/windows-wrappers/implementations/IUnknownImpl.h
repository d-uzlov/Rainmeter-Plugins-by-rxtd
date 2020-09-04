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
#include <Unknwn.h>

namespace rxtd::utils {
	template <typename T>
	class IUnknownImpl : NonMovableBase, VirtualDestructorBase, virtual public T {
		static_assert(std::is_base_of<IUnknown, T>::value, "T must extend IUnknown");
		std::atomic_int referenceCounter{ 1 };

	protected:
		~IUnknownImpl() = default;

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

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvInterface) override {
			if (__uuidof(IUnknown) == riid) {
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
