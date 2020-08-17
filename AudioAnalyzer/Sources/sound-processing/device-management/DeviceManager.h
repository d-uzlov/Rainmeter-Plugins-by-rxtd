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
#include <functional>
#include "RainmeterWrappers.h"

#include "CaptureManager.h"
#include "AudioEnumeratorHelper.h"

namespace rxtd::audio_analyzer {
	class DeviceManager {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		using Logger = utils::Rainmeter::Logger;

	public:
		enum class DataSource {
			eDEFAULT_INPUT,
			eDEFAULT_OUTPUT,
			eID,
		};

		enum class State {
			// usual operating mode
			eOK,
			// something went wrong, but we can fix it
			eERROR_AUTO,
			// something went wrong, and we can't fix it without changing parameters
			// most likely specified device ID not found
			eERROR_MANUAL,
			// something wend wrong, and we can't fix it no matter what, stop trying fixing it
			eFATAL,
		};

	private:
		State state = State::eERROR_MANUAL;

		Logger logger;

		struct {
			DataSource type{ };
			string id;
		} requestedDevice;

		utils::MediaDeviceType sourceDeviceType{ };

		mutable utils::MediaDeviceWrapper audioDeviceHandle{ };

		AudioEnumeratorHelper enumerator;
		CaptureManager captureManager;

		utils::MediaDeviceWrapper::DeviceInfo deviceInfo;

		clock::time_point lastDevicePollTime{ };
		static constexpr clock::duration DEVICE_POLL_TIMEOUT = std::chrono::milliseconds(250);

		const std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback;

	public:
		DeviceManager(Logger logger, std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback);

		~DeviceManager() {
			deviceRelease();
		}

		/** This class is non copyable */
		DeviceManager(const DeviceManager& other) = delete;
		DeviceManager(DeviceManager&& other) = delete;
		DeviceManager& operator=(const DeviceManager& other) = delete;
		DeviceManager& operator=(DeviceManager&& other) = delete;

		[[nodiscard]]
		utils::MediaDeviceType getCurrentDeviceType() const {
			return sourceDeviceType;
		}

		[[nodiscard]]
		State getState() const {
			return state;
		}

		void forceReconnect();

		[[nodiscard]]
		DataSource getRequesterSourceType() const {
			return requestedDevice.type;
		}

		void setOptions(DataSource source, sview deviceID);

		[[nodiscard]]
		const utils::MediaDeviceWrapper::DeviceInfo& getDeviceInfo() const {
			return deviceInfo;
		}

		[[nodiscard]]
		bool getDeviceStatus() const {
			return audioDeviceHandle.isDeviceActive();
		}

		[[nodiscard]]
		CaptureManager& getCaptureManager() {
			return captureManager;
		}

		[[nodiscard]]
		const CaptureManager& getCaptureManager() const {
			return captureManager;
		}

		[[nodiscard]]
		AudioEnumeratorHelper& getDeviceEnumerator() {
			return enumerator;
		}

		[[nodiscard]]
		const AudioEnumeratorHelper& getDeviceEnumerator() const {
			return enumerator;
		}

	private:
		void deviceInit();

		void deviceRelease();
	};

}
