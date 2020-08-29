/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "GenericComWrapper.h"

namespace rxtd::utils {
	class PropertyStoreWrapper : public GenericComWrapper<IPropertyStore> {
	public:
		template <typename InitFunction>
		PropertyStoreWrapper(InitFunction initFunction) : GenericComWrapper(std::move(initFunction)) {
		}

		[[nodiscard]]
		string readPropertyString(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (getPointer()->GetValue(key, prop.getMetaPointer()) != S_OK) {
				return { };
			}

			return { prop.getCString() };
		}

		[[nodiscard]]
		index readPropertyInt(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (getPointer()->GetValue(key, prop.getMetaPointer()) != S_OK) {
				return { };
			}

			return prop.getInt();
		}

		template <typename BlobObjectType>
		[[nodiscard]]
		BlobObjectType readPropertyBlob(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (getPointer()->GetValue(key, prop.getMetaPointer()) != S_OK) {
				return { };
			}

			const BlobObjectType& format = reinterpret_cast<const BlobObjectType&>(prop.getPointer().blob);
			const BlobObjectType result = format;
			return result;
		}

	private:
		class PropVariantWrapper : NonMovableBase {
			PROPVARIANT handle{ };

		public:
			PropVariantWrapper() {
				PropVariantInit(&handle);
			}

			~PropVariantWrapper() {
				PropVariantClear(&handle);
			}

			[[nodiscard]]
			PROPVARIANT* getMetaPointer() {
				return &handle;
			}

			[[nodiscard]]
			const PROPVARIANT& getPointer() const {
				return handle;
			}

			[[nodiscard]]
			const wchar_t* getCString() const {
				return handle.pwszVal;
			}

			[[nodiscard]]
			index getInt() const {
				return handle.uintVal;
			}
		};

	};
}
