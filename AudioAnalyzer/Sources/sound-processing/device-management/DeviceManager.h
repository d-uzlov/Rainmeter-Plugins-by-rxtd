/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "MyWaveFormat.h"
#include "windows-wrappers/BufferWrapper.h"
#include <functional>
#include "RainmeterWrappers.h"

#include "CaptureManager.h"
#include "AudioEnumeratorWrapper.h"
#include "Port.h"

namespace rxtd::audio_analyzer {
	class DeviceManager {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

	private:
		
		bool objectIsValid = true;

		utils::Rainmeter::Logger& logger;

		Port port = Port::eOUTPUT;
		string deviceID;

		AudioEnumeratorWrapper enumerator;
		mutable utils::MediaDeviceWrapper audioDeviceHandle { };

		CaptureManager captureManager;

		utils::MediaDeviceWrapper::DeviceInfo deviceInfo;

		clock::time_point lastDevicePollTime { };
		static constexpr clock::duration DEVICE_POLL_TIMEOUT = std::chrono::milliseconds(250);

		const std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback;

	public:
		DeviceManager(utils::Rainmeter::Logger& logger, std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback);

		~DeviceManager();
		/** This class is non copyable */
		DeviceManager(const DeviceManager& other) = delete;
		DeviceManager(DeviceManager&& other) = delete;
		DeviceManager& operator=(const DeviceManager& other) = delete;
		DeviceManager& operator=(DeviceManager&& other) = delete;

		bool isObjectValid() const;

		void setOptions(Port port, sview deviceID);
		void init();
		bool actualizeDevice(); // returns true if changed
		CaptureManager::BufferFetchResult nextBuffer();

		const string& getDeviceName() const;
		const string& getDeviceId() const;
		const string& getDeviceListLegacy() const;
		const string& getDeviceList2() const;
		bool getDeviceStatus() const;
		const string& getDeviceFormat() const;

		void updateDeviceList();

	private:
		void deviceInit();
		bool acquireDeviceHandle();

		void readDeviceInfo();

		void ensureDeviceAcquired();

		void deviceRelease();
	};

}
