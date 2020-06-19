#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>
#include "PropertyStoreWrapper.h"

const IID IID_IAudioClient = __uuidof(IAudioClient);

namespace rxtd::utils {
	MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
		if (!isValid()) {
			return { };
		}

		PropertyStoreWrapper props;
		lastResult = (*this)->OpenPropertyStore(STGM_READ, &props);
		if (lastResult != S_OK) {
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
		IAudioClientWrapper audioClient;
		lastResult = (*this)->Activate(
			IID_IAudioClient,
			CLSCTX_ALL,
			nullptr,
			reinterpret_cast<void**>(&audioClient)
		);
		return audioClient;
	}

	index MediaDeviceWrapper::getLastResult() const {
		return lastResult;
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
