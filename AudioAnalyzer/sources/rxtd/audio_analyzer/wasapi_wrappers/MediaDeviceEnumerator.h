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

		/// <summary>
		/// Throws ComException on error.
		/// </summary>
		/// <param name="id">Id of the required device</param>
		/// <returns>Valid MediaDeviceHandle object</returns>
		[[nodiscard]]
		MediaDeviceHandle getDeviceByID(const string& id);

		/// <summary>
		/// Throws ComException on error.
		/// </summary>
		/// <param name="type">Type of the required device</param>
		/// <returns>Valid MediaDeviceHandle object</returns>
		[[nodiscard]]
		MediaDeviceHandle getDefaultDevice(MediaDeviceType type);

		/// <summary>
		/// Throws ComException on error.
		/// </summary>
		/// <param name="type">Type of the required devices</param>
		/// <returns>List of valid MediaDeviceHandle objects</returns>
		[[nodiscard]]
		std::vector<MediaDeviceHandle> getActiveDevices(MediaDeviceType type);

	private:
		[[nodiscard]]
		std::vector<MediaDeviceHandle> getCollection(MediaDeviceType type, uint32_t deviceStateMask);
	};
}
