/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <combaseapi.h>

namespace rxtd::utils {
	class PropVariantWrapper {
		PROPVARIANT	handle { };

	public:

		PropVariantWrapper() {
			PropVariantInit(&handle);
		}

		PropVariantWrapper(const PropVariantWrapper& other) = delete;
		PropVariantWrapper(PropVariantWrapper&& other) = delete;
		PropVariantWrapper& operator=(const PropVariantWrapper& other) = delete;
		PropVariantWrapper& operator=(PropVariantWrapper&& other) = delete;

		~PropVariantWrapper() {
			PropVariantClear(&handle);
		}

		PROPVARIANT* operator &() {
			return &handle;
		}
		const wchar_t* getCString() const {
			return handle.pwszVal;
		}
	};
}
