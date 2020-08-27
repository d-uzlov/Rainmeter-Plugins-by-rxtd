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
			CaptureManager::DataSource type{ };
			string id;

			friend bool operator==(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return lhs.type == rhs.type
					&& lhs.id == rhs.id;
			}

			friend bool operator!=(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			ProcessingOrchestrator::Snapshot dataSnapshot;
			CaptureManager::Snapshot deviceInfoSnapshot;

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
			utils::CMMNotificationClient notificationClient;
			std::atomic_bool stopRequest{ false };
			std::atomic_bool paramsWereUpdated{ false };
		} threadSafeFields;

		struct {
			std::mutex mutex;
			std::condition_variable sleepVariable;

			utils::Rainmeter::Logger logger;
			CaptureManager captureManager;
			ProcessingOrchestrator orchestrator;
			RequestedDeviceDescription requestedSource;
		} mainFields;

		struct {
			std::mutex mutex;

			std::thread thread;
			bool needToInitializeThread = false;

			ParamParser::ProcessingsInfoMap patches;
			bool patchesWereUpdated = false;
			RequestedDeviceDescription requestedSource;
			bool sourceWasUpdated = false;

			Snapshot snapshot;
			bool snapshotIsUpdated = false;
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

		void setParams(
			std::optional<RequestedDeviceDescription> request,
			std::optional<ParamParser::ProcessingsInfoMap> _patches
		);

		void setInvalid();

		void update(Snapshot& snap);

	private:
		void wakeThreadUp();
		void stopThread();
		void threadFunction();

		void pUpdate();
		// returns true if ProcessingOrchestrator was updated, false otherwise
		void updateDevice();
		void updateDeviceListStrings();

		std::unique_lock<std::mutex> getMainLock();
		std::unique_lock<std::mutex> getRequestLock();
	};
}
