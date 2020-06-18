#include "PropertyStoreWrapper.h"

namespace rxtd::utils {
	namespace {
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

	string PropertyStoreWrapper::readProperty(const PROPERTYKEY& key) {
		PropVariantWrapper prop;

		if ((*this)->GetValue(key, &prop) != S_OK) {
			return {};
		}

		return prop.getCString();
	}
}

