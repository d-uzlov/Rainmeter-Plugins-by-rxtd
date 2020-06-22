#include "IMMDeviceEnumeratorWrapper.h"

static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

namespace rxtd::utils {

	IMMDeviceEnumeratorWrapper::IMMDeviceEnumeratorWrapper() {
		lastResult = CoCreateInstance(
			CLSID_MMDeviceEnumerator,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_IMMDeviceEnumerator,
			reinterpret_cast<void**>(getMetaPointer())
		);

		if (lastResult != S_OK) {
			release();
			return;
		}
	}

	index IMMDeviceEnumeratorWrapper::getLastResult() const {
		return lastResult;
	}

	MediaDeviceWrapper IMMDeviceEnumeratorWrapper::getDeviceByID(MediaDeviceType type, const string& id) {
		MediaDeviceWrapper audioDeviceHandle { type };

		lastResult = (*this)->GetDevice(id.c_str(), audioDeviceHandle.getMetaPointer());

		return audioDeviceHandle;
	}

	MediaDeviceWrapper IMMDeviceEnumeratorWrapper::getDefaultDevice(MediaDeviceType type) {
		MediaDeviceWrapper audioDeviceHandle { type };

		lastResult = (*this)->GetDefaultAudioEndpoint(type == MediaDeviceType::eOUTPUT ? eRender : eCapture, eConsole, audioDeviceHandle.getMetaPointer());

		return audioDeviceHandle;
	}

	IMMDeviceCollectionWrapper IMMDeviceEnumeratorWrapper::enumerateActiveDevices(MediaDeviceType type) {
		return IMMDeviceCollectionWrapper { type, *this, DEVICE_STATE_ACTIVE };
	}

	IMMDeviceCollectionWrapper IMMDeviceEnumeratorWrapper::enumerateAllDevices(MediaDeviceType type) {
		return IMMDeviceCollectionWrapper { type, *this, DEVICE_STATEMASK_ALL };
	}

}
