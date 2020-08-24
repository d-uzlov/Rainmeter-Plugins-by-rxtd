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
			string formatString;
			utils::MediaDeviceType type{ };
			MyWaveFormat format;
		};

	private:
		State state = State::eERROR_MANUAL;

		Logger logger;

		utils::MediaDeviceWrapper audioDeviceHandle{ };

		CaptureManager captureManager;

		DeviceInfoSnapshot diSnapshot;

		index legacyNumber = 0;

	public:
		void setLegacyNumber(index value) {
			legacyNumber = value;
		}

		void setLogger(Logger value) {
			logger = std::move(value);
		}

		// returns false on fatal error, true otherwise
		[[nodiscard]]
		bool reconnect(AudioEnumeratorHelper& enumerator, DataSource type, const string& id);

		void updateDeviceInfoSnapshot(DeviceInfoSnapshot& snapshot) const {
			snapshot = diSnapshot;
		}

		[[nodiscard]]
		bool isValid() const {
			return state == State::eOK;
		}

		[[nodiscard]]
		CaptureManager& getCaptureManager() {
			return captureManager;
		}

	private:
		void deviceRelease();
	};

}
