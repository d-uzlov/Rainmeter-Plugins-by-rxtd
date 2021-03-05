// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

 // my-windows must be before any WINAPI include
#include "rxtd/my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <mmdeviceapi.h>

#include "MediaDeviceHandle.h"
#include "rxtd/winapi_wrappers/GenericComWrapper.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	class MediaDeviceEnumerator : public winapi_wrappers::GenericComWrapper<IMMDeviceEnumerator> {
	public:
		MediaDeviceEnumerator();

		[[nodiscard]]
		std::optional<MediaDeviceHandle> getDeviceByID(const string& id);

		[[nodiscard]]
		std::optional<MediaDeviceHandle> getDefaultDevice(MediaDeviceType type);

		[[nodiscard]]
		std::vector<MediaDeviceHandle> getActiveDevices(MediaDeviceType type);

	private:
		[[nodiscard]]
		std::vector<MediaDeviceHandle> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
