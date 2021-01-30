﻿/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "DataWithLock.h"
#include "rainmeter/Rainmeter.h"
#include "sound-processing/ProcessingManager.h"
#include "sound-processing/device-management/CaptureManager.h"
#include "sound-processing/ProcessingOrchestrator.h"
#include "wasapi-wrappers/implementations/NotificationClientImpl.h"

namespace rxtd::audio_analyzer {
	class ParentHelper : MovableOnlyBase {
	public:
		using Rainmeter = ::rxtd::common::rainmeter::Rainmeter;
		using Logger = ::rxtd::common::rainmeter::Logger;
		using MediaDeviceType = wasapi_wrappers::MediaDeviceType;

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
				data.useLocking = value;
				deviceInfo.useLocking = value;
				deviceLists.useLocking = value;
			}
		};

		struct Callbacks {
			string onUpdate;
			string onDeviceChange;
			string onDeviceListChange;
			string onDeviceDisconnected;

			// autogenerated
			friend bool operator==(const Callbacks& lhs, const Callbacks& rhs) {
				return lhs.onUpdate == rhs.onUpdate
					&& lhs.onDeviceChange == rhs.onDeviceChange
					&& lhs.onDeviceListChange == rhs.onDeviceListChange
					&& lhs.onDeviceDisconnected == rhs.onDeviceDisconnected;
			}

			friend bool operator!=(const Callbacks& lhs, const Callbacks& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		wasapi_wrappers::IMMDeviceEnumeratorWrapper enumeratorWrapper;

		struct {
			index legacyNumber{};
			bool useThreading = false;
			double updateTime{};
		} constFields;

		struct {
			common::winapi_wrappers::GenericComWrapper<wasapi_wrappers::MediaDeviceListNotificationClient> notificationClient;
		} threadSafeFields;

		struct ThreadSleepFields : DataWithLock {
			std::condition_variable sleepVariable;
			bool stopRequest = false;
			bool updateRequest = false;
		} threadSleepFields;

		struct MainFields {
			Rainmeter rain;
			Logger logger;
			CaptureManager captureManager;
			ProcessingOrchestrator orchestrator;

			struct {
				CaptureManager::SourceDesc device;
				ParamParser::ProcessingsInfoMap patches;
			} settings;

			Callbacks callbacks;
			bool disconnected = false;
		} mainFields;

		struct RequestFields : DataWithLock {
			std::thread thread;

			struct {
				std::optional<Callbacks> callbacks;
				std::optional<CaptureManager::SourceDesc> device;
				std::optional<ParamParser::ProcessingsInfoMap> patches;
			} settings;

			bool disconnect = false;
		} requestFields;

		SnapshotStruct snapshot;

	public:
		~ParentHelper();

		// throws std::runtime_error on fatal error
		void init(
			Rainmeter _rain,
			Logger _logger,
			const utils::OptionMap& threadingMap,
			index _legacyNumber
		);

		void setInvalid();

		void setParams(
			std::optional<Callbacks> callbacks,
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
		bool updateDeviceListStrings();

		string makeDeviceListString(MediaDeviceType type);
		string legacy_makeDeviceListString(MediaDeviceType type);
	};
}
