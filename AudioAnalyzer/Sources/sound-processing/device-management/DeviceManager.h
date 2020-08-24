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

		struct DeviceInfoSnapshot {
			bool status{ };
			string name;
			string description;
			string id;
			string format;
			utils::MediaDeviceType type{ };
		};

	private:
		State state = State::eERROR_MANUAL;

		Logger logger;

		struct {
			DataSource type{ };
			string id;
		} requestedDevice;

		mutable utils::MediaDeviceWrapper audioDeviceHandle{ };

		AudioEnumeratorHelper enumerator;
		CaptureManager captureManager;

		const std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback;

		DeviceInfoSnapshot diSnapshot;

		index legacyNumber = 0;

	public:
		DeviceManager(std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback);

		/** This class is non copyable */
		DeviceManager(const DeviceManager& other) = delete;
		DeviceManager(DeviceManager&& other) = delete;
		DeviceManager& operator=(const DeviceManager& other) = delete;
		DeviceManager& operator=(DeviceManager&& other) = delete;

		void setLegacyNumber(index value) {
			legacyNumber = value;
		}

		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void updateDeviceInfoSnapshot(DeviceInfoSnapshot& snapshot) const {
			snapshot = diSnapshot;
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
		CaptureManager& getCaptureManager() {
			return captureManager;
		}

		[[nodiscard]]
		AudioEnumeratorHelper& getDeviceEnumerator() {
			return enumerator;
		}

	private:
		void deviceInit();

		void deviceRelease();
	};

}
