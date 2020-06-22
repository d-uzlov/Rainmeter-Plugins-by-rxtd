#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>
#include "PropertyStoreWrapper.h"

static const IID IID_IAudioClient = __uuidof(IAudioClient);

namespace rxtd::utils {
	MediaDeviceWrapper::MediaDeviceWrapper(MediaDeviceType type): type(type) {
	}

	MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
		if (!isValid()) {
			return { };
		}

		PropertyStoreWrapper props { *this };
		if (!props.isValid()) {
			return { };
		}

		DeviceInfo deviceInfo;
		deviceInfo.id = readDeviceId();
		deviceInfo.fullFriendlyName = props.readProperty(PKEY_Device_FriendlyName);
		deviceInfo.desc = props.readProperty(PKEY_Device_DeviceDesc);
		deviceInfo.name = props.readProperty(PKEY_DeviceInterface_FriendlyName);

		return deviceInfo;
	}

	string MediaDeviceWrapper::readDeviceId() {
		if (!isValid()) {
			return { };
		}

		wchar_t* resultCString = nullptr;
		lastResult = (*this)->GetId(&resultCString);
		if (lastResult != S_OK) {
			return { };
		}
		string id = resultCString;

		CoTaskMemFree(resultCString);

		return id;
	}

	IAudioClientWrapper MediaDeviceWrapper::openAudioClient() {
		IAudioClientWrapper audioClient { type };
		lastResult = (*this)->Activate(
			IID_IAudioClient,
			CLSCTX_ALL,
			nullptr,
			reinterpret_cast<void**>(audioClient.getMetaPointer())
		);
		return audioClient;
	}

	index MediaDeviceWrapper::getLastResult() const {
		return lastResult;
	}

	MediaDeviceType MediaDeviceWrapper::getType() const {
		return type;
	}

	bool MediaDeviceWrapper::isDeviceActive() {
		if (!isValid()) {
			return false;
		}

		// static_assert(std::is_same<DWORD, uint32_t>::value);
		// ...
		// unsigned != unsigned long
		// I wish I could use proper types everywhere :(

		DWORD state;
		const auto result = (*this)->GetState(&state);
		if (result != S_OK) {
			return false;
		}

		return state == DEVICE_STATE_ACTIVE;
	}
}
