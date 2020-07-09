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
#include <mmdeviceapi.h>

namespace rxtd::utils {
	class PropertyStoreWrapper : public GenericComWrapper<IPropertyStore> {
	public:
		explicit PropertyStoreWrapper(InitFunction initFunction);

		// I got bored of manually creating and freeing prop wrappers, so I created this function
		string readProperty(const PROPERTYKEY& key);
	};
}
