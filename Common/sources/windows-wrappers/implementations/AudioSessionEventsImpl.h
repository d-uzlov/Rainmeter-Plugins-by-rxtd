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
			bool volumeChanged;
			bool channelVolumeChanged;
			DisconnectionReason disconnectionReason{ };
		};

	private:
		IAudioSessionControl* control = nullptr;

		std::atomic<bool> volumeChanged{ };
		std::atomic<bool> channelVolumeChanged{ };
		std::atomic<DisconnectionReason> disconnectionReason{ };

		std::mutex mut;

	public:
		AudioSessionEventsImpl() = default;

		virtual ~AudioSessionEventsImpl() {
			unregister();
		}

		AudioSessionEventsImpl(const AudioSessionEventsImpl& other) = delete;
		AudioSessionEventsImpl(AudioSessionEventsImpl&& other) noexcept = delete;
		AudioSessionEventsImpl& operator=(const AudioSessionEventsImpl& other) = delete;
		AudioSessionEventsImpl& operator=(AudioSessionEventsImpl&& other) noexcept = delete;

		void init(IAudioSessionControl* _control) {
			unregister();
			control = _control;
			control->RegisterAudioSessionNotification(this);
		}

		void unregister() {
			if (control != nullptr) {
				control->UnregisterAudioSessionNotification(this);
				control = nullptr;
			}
		}

		[[nodiscard]]
		Changes takeChanges() {
			std::lock_guard<std::mutex> lock{ mut };
			Changes result{ };
			result.volumeChanged = volumeChanged.exchange(false);
			result.channelVolumeChanged = channelVolumeChanged.exchange(false);
			result.disconnectionReason = disconnectionReason.exchange(DisconnectionReason::eNONE);
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

		HRESULT STDMETHODCALLTYPE
		OnDisplayNameChanged(const wchar_t* NewDisplayName, const GUID* EventContext) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnIconPathChanged(const wchar_t* NewIconPath, const GUID* EventContext) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE
		OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, const GUID* EventContext) override {
			volumeChanged.exchange(true);
			Rainmeter::sourcelessLog(L"OnSimpleVolumeChanged");
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(
			DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel,
			const GUID* EventContext
		) override {
			channelVolumeChanged.exchange(true);
			Rainmeter::sourcelessLog(L"OnChannelVolumeChanged");
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE
		OnGroupingParamChanged(const GUID* NewGroupingParam, const GUID* EventContext) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override {
			switch (DisconnectReason) {
			case DisconnectReasonDeviceRemoval:
			case DisconnectReasonServerShutdown:
			case DisconnectReasonSessionLogoff:
			case DisconnectReasonSessionDisconnected:
				disconnectionReason.exchange(DisconnectionReason::eUNAVAILABLE);
				break;
			case DisconnectReasonFormatChanged:
				disconnectionReason.exchange(DisconnectionReason::eRECONNECT);
				break;
			case DisconnectReasonExclusiveModeOverride:
				disconnectionReason.exchange(DisconnectionReason::eEXCLUSIVE);
				break;
			}
			Rainmeter::sourcelessLog(L"OnSessionDisconnected");

			return S_OK;
		}
	};
}
