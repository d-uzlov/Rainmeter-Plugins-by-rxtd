#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "IAudioClientWrapper.h"
#include "MediaDeviceType.h"

namespace rxtd::utils {

	class MediaDeviceWrapper : public GenericComWrapper<IMMDevice> {
	public:
		struct DeviceInfo {
			string id;
			string name;
			string desc;
			string fullFriendlyName;
		};

	private:

		MediaDeviceType type;
		index lastResult = { };

	public:
		MediaDeviceWrapper() = default;

		explicit MediaDeviceWrapper(MediaDeviceType type);

		DeviceInfo readDeviceInfo();
		string readDeviceId();

		IAudioClientWrapper openAudioClient();

		index getLastResult() const;

		MediaDeviceType getType() const;

		bool isDeviceActive();
	};
}