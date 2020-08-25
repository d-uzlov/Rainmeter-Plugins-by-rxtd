/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PropertyStoreWrapper.h"
#include "MediaDeviceWrapper.h"

using namespace utils;

namespace {
	class PropVariantWrapper {
		PROPVARIANT handle{ };

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

		[[nodiscard]]
		PROPVARIANT* getMetaPointer() {
			return &handle;
		}

		[[nodiscard]]
		const wchar_t* getCString() const {
			return handle.pwszVal;
		}
	};
}

PropertyStoreWrapper::PropertyStoreWrapper(InitFunction initFunction) : GenericComWrapper(std::move(initFunction)) {
}

string PropertyStoreWrapper::readProperty(const PROPERTYKEY& key) {
	PropVariantWrapper prop;

	if (getPointer()->GetValue(key, prop.getMetaPointer()) != S_OK) {
		return { };
	}

	return { prop.getCString() };
}
