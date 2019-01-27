/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "my-windows.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include "MyWaveFormat.h"
#include "windows-wrappers/BufferWrapper.h"
#include <functional>
#include "RainmeterWrappers.h"
#include "windows-wrappers/GenericComWrapper.h"
#include <chrono>
#include <variant>

#undef NO_DATA

namespace rxaa {
	class DeviceManager {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

	public:
		enum class Port {
			OUTPUT,
			INPUT,
		};

		enum class BufferFetchState {
			OK,
			NO_DATA,
			DEVICE_ERROR,
			INVALID_STATE,
		};

		class BufferFetchResult {
			const BufferFetchState state;
			const utils::BufferWrapper buffer;

			explicit BufferFetchResult(BufferFetchState state)
				: state(state), buffer(utils::BufferWrapper()) { }

		public:
			static BufferFetchResult noData() {
				return BufferFetchResult(BufferFetchState::NO_DATA);
			}
			static BufferFetchResult deviceError() {
				return BufferFetchResult(BufferFetchState::DEVICE_ERROR);
			}
			static BufferFetchResult invalidState() {
				return BufferFetchResult(BufferFetchState::INVALID_STATE);
			}

			BufferFetchResult(utils::BufferWrapper&& buffer)
				: state(BufferFetchState::OK), buffer(std::move(buffer)) { }

			BufferFetchState getState() const {
				return state;
			}
			const utils::BufferWrapper& getBuffer() const {
				return buffer;
			}
		};

	private:
		/** AudioLevel uses silent renderer to prevent stuttering
		 * Details of the issue can be found here:
		 * http://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/c7ba0a04-46ce-43ff-ad15-ce8932c00171/loopback-recording-causes-digital-stuttering?forum=windowspro-audiodevelopment
		 * I temporary disable this as I don't seem to have this problem.
		 */
		class SilentRenderer {
			IAudioClient *audioClient = nullptr;
			IAudioRenderClient *renderer = nullptr;

		public:
			SilentRenderer() = default;
			SilentRenderer(IMMDevice *audioDeviceHandle);

			SilentRenderer(SilentRenderer&& other) noexcept;
			SilentRenderer& operator=(SilentRenderer&& other) noexcept;

			SilentRenderer(const SilentRenderer& other) = delete;
			SilentRenderer& operator=(const SilentRenderer& other) = delete;

			~SilentRenderer();

			bool isError() const;

		private:
			void releaseHandles();
		};

		class CaptureManager {
		public:
			struct Error {
				bool temporary;
				string explanation;
				int32_t errorCode;

				Error(bool temporary, string explanation, int32_t errorCode) :
					temporary(temporary), explanation(std::move(explanation)), errorCode(errorCode) {
				}
			};
		private:
			utils::GenericComWrapper<IAudioClient> audioClient { };
			utils::GenericComWrapper<IAudioCaptureClient> audioCaptureClient { };
			MyWaveFormat waveFormat { };

		public:
			static std::variant<CaptureManager, Error> create(IMMDevice& audioDeviceHandle, bool loopback);

			CaptureManager() = default;
			~CaptureManager();

			CaptureManager(CaptureManager&& other) noexcept = default;
			CaptureManager& operator=(CaptureManager&& other) noexcept = default;

			CaptureManager(const CaptureManager& other) = delete;
			CaptureManager& operator=(const CaptureManager& other) = delete;

			MyWaveFormat getWaveFormat() const;
			bool isEmpty() const;
			bool isValid() const;
			utils::BufferWrapper readBuffer();

		private:
			void releaseHandles();
		};

		bool objectIsValid = true;

		utils::Rainmeter::Logger& logger;

		Port port = Port::OUTPUT;
		string deviceID;

		utils::GenericComWrapper<IMMDeviceEnumerator> audioEnumeratorHandle;

		IMMDevice *audioDeviceHandle { };

		SilentRenderer silentRenderer;
		CaptureManager captureManager;

		struct {
			string name;
			string id;
			string format;

			void reset() {
				name.clear();
				id.clear();
				format.clear();
			}
		} deviceInfo;

		string deviceList;

		clock::time_point lastBufferFillTime { };
		clock::time_point lastDevicePollTime { };
		static constexpr clock::duration DEVICE_POLL_TIMEOUT = std::chrono::milliseconds(250);
		static constexpr clock::duration EMPTY_TIMEOUT = std::chrono::milliseconds(100);

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
		BufferFetchResult nextBuffer();

		const string& getDeviceName() const;
		const string& getDeviceId() const;
		const string& getDeviceList() const;
		void updateDeviceList();
		bool getDeviceStatus() const;
		const string& getDeviceFormat() const;

	private:
		void deviceInit();
		bool acquireDeviceHandle();
		static std::optional<MyWaveFormat> parseStreamFormat(WAVEFORMATEX* waveFormatEx);

		void readDeviceInfo();
		void readDeviceName();
		void readDeviceId();
		void readDeviceFormat();

		bool createSilentRenderer();
		bool createCaptureManager();

		void ensureDeviceAcquired();

		void deviceRelease();
	};

}
