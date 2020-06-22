#include "PropertyStoreWrapper.h"
#include "MediaDeviceWrapper.h"

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

	PropertyStoreWrapper::PropertyStoreWrapper(MediaDeviceWrapper& device) {
		const auto result = device->OpenPropertyStore(STGM_READ, getMetaPointer());

		if (result != S_OK) {
			release();
			return;
		}
	}

	string PropertyStoreWrapper::readProperty(const PROPERTYKEY& key) {
		PropVariantWrapper prop;

		if ((*this)->GetValue(key, &prop) != S_OK) {
			return {};
		}

		GenericComWrapper<IUnknown> test;

		return prop.getCString();
	}
}

