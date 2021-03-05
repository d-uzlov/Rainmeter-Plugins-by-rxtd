// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

#include "GenericComWrapper.h"
#include "propsys.h"

namespace rxtd::winapi_wrappers {
	class PropertyStoreWrapper : public GenericComWrapper<IPropertyStore> {
	public:
		template<typename InitFunction>
		PropertyStoreWrapper(InitFunction initFunction) : GenericComWrapper(std::move(initFunction)) { }

		[[nodiscard]]
		std::optional<string> readPropertyString(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (ref().GetValue(key, prop.getMetaPointer()) != S_OK) {
				return {};
			}

			return string{ prop.getCString() };
		}

		template<typename ResultType>
		[[nodiscard]]
		std::optional<ResultType> readPropertyInt(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (ref().GetValue(key, prop.getMetaPointer()) != S_OK) {
				return {};
			}

			return static_cast<ResultType>(prop.getInt());
		}

		template<typename BlobObjectType>
		[[nodiscard]]
		std::optional<BlobObjectType> readPropertyBlob(const PROPERTYKEY& key) {
			PropVariantWrapper prop;

			if (ref().GetValue(key, prop.getMetaPointer()) != S_OK) {
				return {};
			}

			const BlobObjectType result = reinterpret_cast<const BlobObjectType&>(prop.ref().blob);
			return result;
		}

	private:
		class PropVariantWrapper : NonMovableBase {
			PROPVARIANT handle{};

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
			const PROPVARIANT& ref() const {
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
