#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>

namespace rxtd::utils {
	class MediaDeviceWrapper : public GenericComWrapper<IMMDevice> {
	public:
		struct DeviceInfo {
			string id;
			string name;
			string desc;
			string fullFriendlyName;
		};

		DeviceInfo readDeviceInfo();
		string readDeviceId();

		bool isDeviceActive();
	};
}