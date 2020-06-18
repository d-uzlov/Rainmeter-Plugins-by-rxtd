#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>

namespace rxtd::utils {
	class PropertyStoreWrapper : public GenericComWrapper<IPropertyStore> {
	public:
		// I got bored of manually creating and freeing prop wrappers, so I created this function
		string readProperty(const PROPERTYKEY& key);
	};
}
