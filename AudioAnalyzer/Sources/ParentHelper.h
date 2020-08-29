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
	class ParentHelper : MovableOnlyBase {
	public:
		struct DataWithLock {
			bool useThreading{ };
			std::mutex mutex;

			std::unique_lock<std::mutex> getLock() {
				auto lock = std::unique_lock<std::mutex>{ mutex, std::defer_lock };
				if (useThreading) {
					lock.lock();
				}
				return lock;
			}
		};

		struct SnapshotStruct {
			struct LockableData : DataWithLock {
				ProcessingOrchestrator::Snapshot _;
			} data;

			struct LockableDeviceInfo : DataWithLock {
				CaptureManager::Snapshot _;
			} deviceInfo;

			struct LockableDeviceListStrings : DataWithLock {
				string input;
				string output;
			} deviceLists;

			std::atomic<bool> deviceIsAvailable{ false };

			void setThreading(bool value) {
				data.useThreading = value;
				deviceInfo.useThreading = value;
				deviceLists.useThreading = value;
			}
		};

	private:
		utils::IMMDeviceEnumeratorWrapper enumeratorWrapper;

		struct {
			index legacyNumber{ };
			bool useThreading = false;
			double updateTime{ };
		} constFields;

		struct {
			utils::CMMNotificationClient notificationClient;
			std::atomic_bool stopRequest{ false };
		} threadSafeFields;

		struct MainFields : DataWithLock {
			std::condition_variable sleepVariable;

			utils::Rainmeter::Logger logger;
			CaptureManager captureManager;
			ProcessingOrchestrator orchestrator;

			struct {
				CaptureManager::SourceDesc device;
				ParamParser::ProcessingsInfoMap patches;
			} settings;
		} mainFields;

		struct RequestFields : DataWithLock {
			std::thread thread;

			struct {
				std::optional<CaptureManager::SourceDesc> device;
				std::optional<ParamParser::ProcessingsInfoMap> patches;
			} settings;
		} requestFields;

		SnapshotStruct snapshot;

	public:
		~ParentHelper();

		// throws std::exception on fatal error
		void init(
			utils::Rainmeter::Logger _logger,
			const utils::OptionMap& threadingMap,
			index _legacyNumber
		);

		void setInvalid();

		void setParams(
			std::optional<CaptureManager::SourceDesc> device,
			std::optional<ParamParser::ProcessingsInfoMap> patches
		);

		SnapshotStruct& getSnapshot() {
			return snapshot;
		}

		void update();

	private:
		void wakeThreadUp();

		// Caller of this function must *NOT* have RequestLock
		// this function joins the thread, which might be waiting on it before finishing
		void stopThread();
		void threadFunction();

		void pUpdate();
		// returns true device format changed, false otherwise
		bool reconnectToDevice();
		void updateProcessings();
		void updateDeviceListStrings();

		string makeDeviceListString(utils::MediaDeviceType type);
		string legacy_makeDeviceListString(utils::MediaDeviceType type);
	};
}
