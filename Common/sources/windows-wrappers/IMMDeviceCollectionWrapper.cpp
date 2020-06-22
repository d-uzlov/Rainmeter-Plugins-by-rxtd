#include "IMMDeviceCollectionWrapper.h"
#include "IMMDeviceEnumeratorWrapper.h"

namespace rxtd::utils {

	IMMDeviceCollectionWrapper::IMMDeviceCollectionWrapper(MediaDeviceType type, IMMDeviceEnumeratorWrapper& enumerator, uint32_t deviceStateMask): 
		type(type) {

		const auto lastResult = enumerator->EnumAudioEndpoints(type == MediaDeviceType::eOUTPUT ? eRender : eCapture, deviceStateMask, getMetaPointer());
		if (lastResult != S_OK) {
			release();
			return;
		}

		static_assert(std::is_same<UINT, uint32_t>::value);

		uint32_t devicesCountUINT;
		(*this)->GetCount(&devicesCountUINT);
		devicesCount = devicesCountUINT;
	}

	index IMMDeviceCollectionWrapper::getDevicesCount() const {
		return devicesCount;
	}

	MediaDeviceWrapper IMMDeviceCollectionWrapper::get(index i) {
		if (i >= devicesCount) {
			return { };
		}

		MediaDeviceWrapper device { type };

		// according to documentation this method shouldn't ever fail unless args are wrong
		(*this)->Item(UINT(i), device.getMetaPointer());

		return device;
	}

	index IMMDeviceCollectionWrapper::getLastResult() const {
		return lastResult;
	}
}
