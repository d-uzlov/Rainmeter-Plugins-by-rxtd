#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "IAudioClientWrapper.h"

namespace rxtd::utils {
	class MediaDeviceWrapper : public GenericComWrapper<IMMDevice> {
	private:
		index lastResult = { };

	public:
		struct DeviceInfo {
			string id;
			string name;
			string desc;
			string fullFriendlyName;
		};

		DeviceInfo readDeviceInfo();
		string readDeviceId();

		IAudioClientWrapper openAudioClient();

		index getLastResult() const;

		bool isDeviceActive();
	};
}