#pragma once

#include "GenericComWrapper.h"
#include <mmdeviceapi.h>
#include "MediaDeviceWrapper.h"

namespace rxtd::utils {
	enum class MediaDeviceType;

	class IMMDeviceCollectionWrapper : public GenericComWrapper<IMMDeviceCollection> {
	public:
	private:

		MediaDeviceType type;
		index devicesCount = { };
		index lastResult = { };

	public:
		explicit IMMDeviceCollectionWrapper(MediaDeviceType type, IMMDeviceEnumeratorWrapper& enumerator, uint32_t deviceStateMask);

		index getDevicesCount() const;

		MediaDeviceWrapper get(index i);

		index getLastResult() const;
	};
}
