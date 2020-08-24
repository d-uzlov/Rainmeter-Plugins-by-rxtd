/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "RainmeterWrappers.h"
#include "sound-processing/ProcessingManager.h"
#include "sound-processing/device-management/DeviceManager.h"
#include "sound-processing/ProcessingOrchestrator.h"
#include "windows-wrappers/IMMNotificationClientImpl.h"

namespace rxtd::audio_analyzer {
	class ParentHelper {
	public:
		struct RequestedDeviceDescription {
			DeviceManager::DataSource sourceType{ };
			string id;

			friend bool operator==(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return lhs.sourceType == rhs.sourceType
					&& lhs.id == rhs.id;
			}

			friend bool operator!=(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			ProcessingOrchestrator::DataSnapshot dataSnapshot;
			DeviceManager::DeviceInfoSnapshot diSnapshot;

			string deviceListInput;
			string deviceListOutput;
			string legacy_deviceList;

			bool fatalError = false;
		};

	private:
		ChannelMixer channelMixer;
		DeviceManager deviceManager;

		utils::GenericComWrapper<utils::CMMNotificationClient> notificationClient;
		ProcessingOrchestrator orchestrator;
		AudioEnumeratorHelper enumerator;

		utils::Rainmeter::Logger logger;
		index legacyNumber{ };

		RequestedDeviceDescription requestedSource;
		Snapshot snapshot;

		bool formatIsUpdated = false;

	public:
		// return true on success, false on fatal error
		bool init(utils::Rainmeter::Logger _logger, index _legacyNumber, double computeTimeout, double killTimeout);

		void setParams(
			RequestedDeviceDescription request,
			const ParamParser::ProcessingsInfoMap& patches,
			index legacyNumber,
			ProcessingOrchestrator::DataSnapshot& dataSnapshot
		);

		void update(Snapshot& snap);

	private:
		void pUpdate();
		void updateDevice();
	};
}
