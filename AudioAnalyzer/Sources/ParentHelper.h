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

			bool fatalError = false;
			bool deviceIsAvailable = false;
		};

	private:
		ChannelMixer channelMixer;
		DeviceManager deviceManager;

		utils::CMMNotificationClient notificationClient;
		ProcessingOrchestrator orchestrator;
		AudioEnumeratorHelper enumerator;

		index legacyNumber{ };

		bool deviceIsAvailable = true;

		RequestedDeviceDescription requestedSource;

		Snapshot snapshot;
		bool snapshotIsUpdated = false;

		bool useThreading = false;
		bool needToInitializeThread = false;
		std::thread thread;
		std::atomic_bool stopRequest{ false };
		double updateTime{ };

		// #fullStateMutex guards all fields except those guarded by #snapshotMutex
		std::mutex fullStateMutex;
		std::condition_variable sleepVariable;

		// snapshotMutex guards following fields:
		//		#snapshot
		//		#snapshotIsUpdated
		//		#snapshotIsUpdated
		std::mutex snapshotMutex;

	public:
		ParentHelper() = default;

		ParentHelper(const ParentHelper& other) = delete;
		ParentHelper(ParentHelper&& other) noexcept = delete;
		ParentHelper& operator=(const ParentHelper& other) = delete;
		ParentHelper& operator=(ParentHelper&& other) noexcept = delete;

		~ParentHelper();

		// return true on success, false on fatal error
		bool init(
			utils::Rainmeter::Logger _logger,
			utils::OptionMap threadingMap,
			index _legacyNumber
		);

		void setParams(
			RequestedDeviceDescription request,
			const ParamParser::ProcessingsInfoMap& patches,
			index legacyNumber,
			Snapshot& snap
		);

		void update(Snapshot& snap);

	private:
		void separateThreadFunction();

		void pUpdate();
		void updateDevice();
		void updateDeviceListStrings();

		std::unique_lock<std::mutex> getFullStateLock();
		std::unique_lock<std::mutex> getSnapshotLock();
	};
}
