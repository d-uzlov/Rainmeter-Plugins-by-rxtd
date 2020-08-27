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
#include "sound-processing/device-management/CaptureManager.h"
#include "sound-processing/ProcessingOrchestrator.h"
#include "windows-wrappers/implementations/NotificationClientImpl.h"

namespace rxtd::audio_analyzer {
	class ParentHelper {
	public:
		struct RequestedDeviceDescription {
			CaptureManager::DataSource sourceType{ };
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
			CaptureManager::Snapshot diSnapshot;

			string deviceListInput;
			string deviceListOutput;

			bool deviceIsAvailable = false;
		};

	private:
		AudioEnumeratorHelper enumerator;

		struct {
			index legacyNumber{ };
			bool useThreading = false;
			double updateTime{ };
		} constFields;

		struct {
			std::atomic_bool stopRequest{ false };
			utils::CMMNotificationClient notificationClient;
		} threadSafeFields;

		struct {
			std::mutex mutex;
			std::condition_variable sleepVariable;

			utils::Rainmeter::Logger logger;
			CaptureManager captureManager;
			bool needToInitializeThread = false;
			std::thread thread;
			ProcessingOrchestrator orchestrator;
		} mainFields;

		struct {
			std::mutex mutex;

			ParamParser::ProcessingsInfoMap patches;
			RequestedDeviceDescription requestedSource;

			Snapshot snapshot;
			bool snapshotIsUpdated = false;
			bool optionsChanged = false;
		} requestFields;

	public:
		ParentHelper() = default;

		ParentHelper(const ParentHelper& other) = delete;
		ParentHelper(ParentHelper&& other) noexcept = delete;
		ParentHelper& operator=(const ParentHelper& other) = delete;
		ParentHelper& operator=(ParentHelper&& other) noexcept = delete;

		~ParentHelper();

		// throws std::exception on fatal error
		void init(
			utils::Rainmeter::Logger _logger,
			const utils::OptionMap& threadingMap,
			index _legacyNumber
		);

		void setParams(RequestedDeviceDescription request, ParamParser::ProcessingsInfoMap _patches);

		void setInvalid();

		void update(Snapshot& snap);

	private:
		void wakeThreadUp();
		void stopThread();
		void threadFunction();

		void pUpdate();
		void updateDevice();
		void updateDeviceListStrings();

		std::unique_lock<std::mutex> getMainLock();
		std::unique_lock<std::mutex> getRequestLock();
	};
}
