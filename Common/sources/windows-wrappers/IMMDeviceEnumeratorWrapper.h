#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "MediaDeviceWrapper.h"
#include "IMMDeviceCollectionWrapper.h"

namespace rxtd::utils {
	enum class MediaDeviceType;

	class IMMDeviceEnumeratorWrapper : public GenericComWrapper<IMMDeviceEnumerator> {
	public:
	private:

		index lastResult = { };

	public:
		IMMDeviceEnumeratorWrapper();

		index getLastResult() const;

		MediaDeviceWrapper getDeviceByID(MediaDeviceType type, const string& id);
		
		MediaDeviceWrapper getDefaultDevice(MediaDeviceType type);

		IMMDeviceCollectionWrapper enumerateActiveDevices(MediaDeviceType type);
		IMMDeviceCollectionWrapper enumerateAllDevices(MediaDeviceType type);
	};
}
