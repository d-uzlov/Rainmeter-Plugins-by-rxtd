/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "../IUnknownImpl.h"
#include "audiopolicy.h"

#include "RainmeterWrappers.h"

#include <mutex>
#include <atomic>

namespace rxtd::utils {
	class AudioSessionEventsImpl : public IUnknownImpl<IAudioSessionEvents> {
	public:
		enum class DisconnectionReason {
			eNONE,
			eUNAVAILABLE,
			eRECONNECT,
			eEXCLUSIVE,
		};

		struct Changes {
			DisconnectionReason disconnectionReason{ };
		};

	private:
		GenericComWrapper<IAudioSessionControl> sessionController;
		GenericComWrapper<ISimpleAudioVolume> mainVolumeController;
		GenericComWrapper<IChannelAudioVolume> channelVolumeController;

		bool preventVolumeChange = false;

		std::atomic<DisconnectionReason> disconnectionReason{ };

		// I'm not sure, but we are changing the volume when someone else changes volume.
		// We can potentially receive event of our own actions.
		// Due to the use of a mutex deadlock can occur
		// 
		// If events are created separately and not recursively,
		// then this listener in conjunction with some other listener that changes the volume
		// to some value other than 1.0,
		//	then, well, there will be an infinite loop, and I can do nothing with it
		//	press F to pay respect
		std::atomic<bool> recursionAtomic{ false };

		std::mutex mut;

	public:
		AudioSessionEventsImpl() = default;

		AudioSessionEventsImpl(IAudioClientWrapper& audioClient) {
			preventVolumeChange = true;
			sessionController = audioClient.getInterface<IAudioSessionControl>();
			mainVolumeController = audioClient.getInterface<ISimpleAudioVolume>();
			channelVolumeController = audioClient.getInterface<IChannelAudioVolume>();
			sessionController.getPointer()->RegisterAudioSessionNotification(this);
		}

		virtual ~AudioSessionEventsImpl() {
			deinit();
		}

		AudioSessionEventsImpl(const AudioSessionEventsImpl& other) = delete;
		AudioSessionEventsImpl(AudioSessionEventsImpl&& other) noexcept = delete;
		AudioSessionEventsImpl& operator=(const AudioSessionEventsImpl& other) = delete;
		AudioSessionEventsImpl& operator=(AudioSessionEventsImpl&& other) noexcept = delete;

		//	""
		//	When releasing an IAudioSessionControl interface instance,
		//	the client must call the interface's Release method
		//	from the same thread as the call to IAudioClient::GetService
		//	that created the object.
		//	""
		//	Source: https://docs.microsoft.com/en-us/windows/win32/api/audiopolicy/nn-audiopolicy-iaudiosessioncontrol
		//
		//	Destructor of AudioSessionEventsImpl can potentially be called from some other threads,
		//	so AudioSessionEventsImpl has #deinit function,
		//	which must be manually called when this object is no longer needed
		void deinit() {
			std::lock_guard<std::mutex> lock{ mut };

			if (sessionController.isValid()) {
				sessionController.getPointer()->UnregisterAudioSessionNotification(this);
			}

			sessionController = { };
			mainVolumeController = { };
			channelVolumeController = { };
		}

		[[nodiscard]]
		Changes takeChanges() {
			Changes result{ };
			result.disconnectionReason = disconnectionReason.load();
			return result;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvInterface) override {
			const auto result = IUnknownImpl::QueryInterface(riid, ppvInterface);
			if (result == S_OK) {
				return result;
			}

			if (__uuidof(IAudioSessionEvents) == riid) {
				AddRef();
				*ppvInterface = this;
			} else {
				*ppvInterface = nullptr;
				return E_NOINTERFACE;
			}
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(const wchar_t* name, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnIconPathChanged(const wchar_t* iconPath, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float volumeLevel, BOOL isMuted, const GUID* context) override {
			if (!preventVolumeChange) {
				return S_OK;
			}

			if (recursionAtomic.load()) {
				return S_OK;
			}
			recursionAtomic.exchange(true);

			std::lock_guard<std::mutex> lock{ mut };

			if (!mainVolumeController.isValid()) {
				return S_OK;
			}

			if (isMuted != 0) {
				const auto result = mainVolumeController.getPointer()->SetMute(false, nullptr);
				if (result != S_OK) {
					mainVolumeController = { };
				}
			}
			if (mainVolumeController.isValid() && volumeLevel != 1.0f) {
				const auto result = mainVolumeController.getPointer()->SetMasterVolume(1.0f, nullptr);
				if (result != S_OK) {
					mainVolumeController = { };
				}
			}

			recursionAtomic.exchange(false);

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE
		OnChannelVolumeChanged(DWORD channelCount, float volumes[], DWORD channel, const GUID* context) override {
			if (!preventVolumeChange) {
				return S_OK;
			}

			if (recursionAtomic.load()) {
				return S_OK;
			}
			recursionAtomic.exchange(true);

			std::lock_guard<std::mutex> lock{ mut };

			if (!channelVolumeController.isValid()) {
				return S_OK;
			}

			bool otherNoneOne = false;
			for (index i = 0; i < index(channelCount); ++i) {
				if (i != index(channel) && volumes[i] != 1.0f) {
					otherNoneOne = true;
					volumes[i] = 1.0f;
				}
			}

			if (otherNoneOne) {
				volumes[channel] = 1.0f;
				const auto result = channelVolumeController.getPointer()->SetAllVolumes(
					channel, volumes, nullptr
				);
				if (result != S_OK) {
					channelVolumeController = { };
				}
			} else if (volumes[channel] != 1.0f) {
				const auto result = channelVolumeController.getPointer()->SetChannelVolume(
					channel, 1.0f, nullptr
				);
				if (result != S_OK) {
					channelVolumeController = { };
				}
			}

			recursionAtomic.exchange(false);

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(const GUID* groupingParam, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState state) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason reason) override {
			if (disconnectionReason.load() != DisconnectionReason::eNONE) {
				// for some reason DisconnectReasonDeviceRemoval makes 2 calls on my PC
				return S_OK;
			}

			switch (reason) {
			case DisconnectReasonDeviceRemoval:
			case DisconnectReasonServerShutdown:
			case DisconnectReasonSessionLogoff:
			case DisconnectReasonSessionDisconnected:
				disconnectionReason.exchange(DisconnectionReason::eUNAVAILABLE);
				Rainmeter::sourcelessLog(L"OnSessionDisconnected eUNAVAILABLE");
				break;
			case DisconnectReasonFormatChanged:
				disconnectionReason.exchange(DisconnectionReason::eRECONNECT);
				Rainmeter::sourcelessLog(L"OnSessionDisconnected eRECONNECT");
				break;
			case DisconnectReasonExclusiveModeOverride:
				disconnectionReason.exchange(DisconnectionReason::eEXCLUSIVE);
				Rainmeter::sourcelessLog(L"OnSessionDisconnected eEXCLUSIVE");
				break;
			}

			return S_OK;
		}
	};
}
