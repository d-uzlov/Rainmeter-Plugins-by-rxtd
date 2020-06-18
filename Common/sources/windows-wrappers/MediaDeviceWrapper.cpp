#include "MediaDeviceWrapper.h"

#include <functiondiscoverykeys_devpkey.h>
#include "PropertyStoreWrapper.h"

namespace rxtd::utils {
	MediaDeviceWrapper::DeviceInfo MediaDeviceWrapper::readDeviceInfo() {
		if (!isValid()) {
			return {};
		}

		PropertyStoreWrapper props;
		if ((*this)->OpenPropertyStore(STGM_READ, &props) != S_OK) {
			return {};
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
			return {};
		}

		wchar_t *resultCString = nullptr;
		if ((*this)->GetId(&resultCString) != S_OK) {
			return {};
		}
		string id = resultCString;

		CoTaskMemFree(resultCString);

		return id;
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

