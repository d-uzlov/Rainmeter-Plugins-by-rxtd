/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <atomic>
#include <mutex>

#include <audiopolicy.h>

#include "../IAudioClientWrapper.h"
#include "winapi-wrappers/GenericComWrapper.h"
#include "winapi-wrappers/implementations/IUnknownImpl.h"


namespace rxtd::audio_analyzer::wasapi_wrappers {
	class AudioSessionEventsImpl : public common::winapi_wrappers::IUnknownImpl<IAudioSessionEvents> {
	public:
		template<typename T>
		using GenericComWrapper = common::winapi_wrappers::GenericComWrapper<T>;

		enum class DisconnectionReason {
			eNONE,
			eUNAVAILABLE,
			eRECONNECT,
			eEXCLUSIVE,
		};

		struct Changes {
			DisconnectionReason disconnectionReason{};
		};

	private:
		GenericComWrapper<IAudioSessionControl> sessionController;

		// "The client should never release the final reference on a WASAPI object during an event callback."
		// Source: https://docs.microsoft.com/en-us/windows/win32/api/audiopolicy/nn-audiopolicy-iaudiosessionevents
		// So I maintain std::atomic instead of deleting object when it should be invalidated
		std::atomic<bool> mainVolumeControllerIsValid{ false };
		GenericComWrapper<ISimpleAudioVolume> mainVolumeController;
		std::atomic<bool> channelVolumeControllerIsValid{ false };
		GenericComWrapper<IChannelAudioVolume> channelVolumeController;

		bool preventVolumeChange = false;

		std::atomic<DisconnectionReason> disconnectionReason{ DisconnectionReason::eNONE };

		// I'm not sure, but we are changing the volume inside listener on volume changes
		// We can potentially receive event of our own actions
		// (ideally I would research now this really works and not just throw check here to be safe but that requires time which I don't have)
		// Due to the use of a mutex deadlock can occur
		// This atomic prevents this
		// I can't use std::mutex#try_lock for this purpose because it can spuriously fail
		//
		// If events are created separately and not recursively,
		// and there is some other listener that changes the volume
		// to some value other than 1.0,
		// then, well, there will be an infinite loop, and I can do nothing to prevent this
		//    press F to pay respect
		std::atomic<bool> recursionAtomic{ false };

		std::mutex mut;

	public:
		static AudioSessionEventsImpl* create(IAudioClientWrapper& audioClient, bool preventVolumeChange) noexcept(false) {
			AudioSessionEventsImpl& result = *new AudioSessionEventsImpl;
			result.preventVolumeChange = preventVolumeChange;

			result.sessionController = audioClient.getInterface<IAudioSessionControl>();

			if (result.sessionController.isValid()) {
				result.sessionController.throwOnError(
					result.sessionController.ref().RegisterAudioSessionNotification(&result),
					L"IAudioSessionControl.RegisterAudioSessionNotification() in AudioSessionEventsImpl factory"
				);

				result.mainVolumeController = audioClient.getInterface<ISimpleAudioVolume>();
				result.mainVolumeControllerIsValid.exchange(result.mainVolumeController.isValid());

				result.channelVolumeController = audioClient.getInterface<IChannelAudioVolume>();
				result.channelVolumeControllerIsValid.exchange(result.channelVolumeController.isValid());
			}

			return &result;
		}

		static AudioSessionEventsImpl* create(GenericComWrapper<IAudioSessionControl>&& _sessionController, bool preventVolumeChange) noexcept(false) {
			AudioSessionEventsImpl& result = *new AudioSessionEventsImpl;
			result.preventVolumeChange = preventVolumeChange;

			result.sessionController = std::move(_sessionController);
			result.sessionController.throwOnError(
				result.sessionController.ref().RegisterAudioSessionNotification(&result),
				L"IAudioSessionControl.RegisterAudioSessionNotification() in AudioSessionEventsImpl factory"
			);

			return &result;
		}

	private:
		AudioSessionEventsImpl() = default;

	protected:
		virtual ~AudioSessionEventsImpl() {
			deinit();
		}

	public:
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
				sessionController.ref().UnregisterAudioSessionNotification(this);
				sessionController = {};
			}

			mainVolumeController = {};
			mainVolumeControllerIsValid.exchange(mainVolumeController.isValid());

			channelVolumeController = {};
			channelVolumeControllerIsValid.exchange(mainVolumeController.isValid());
		}

		[[nodiscard]]
		Changes takeChanges() {
			Changes result{};
			result.disconnectionReason = disconnectionReason.load();
			return result;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, void** ppvInterface) override {
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
			if (!preventVolumeChange || !mainVolumeControllerIsValid.load()) {
				return S_OK;
			}

			if (recursionAtomic.load()) {
				return S_OK;
			}
			recursionAtomic.exchange(true);

			std::lock_guard<std::mutex> lock{ mut };

			if (isMuted != 0) {
				const auto result = mainVolumeController.ref().SetMute(false, nullptr);
				if (result != S_OK) {
					mainVolumeControllerIsValid.exchange(false);
				}
			}
			if (volumeLevel != 1.0f && mainVolumeControllerIsValid.load()) {
				const auto result = mainVolumeController.ref().SetMasterVolume(1.0f, nullptr);
				if (result != S_OK) {
					mainVolumeControllerIsValid.exchange(false);
				}
			}

			recursionAtomic.exchange(false);

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE
		OnChannelVolumeChanged(DWORD channelCount, float volumes[], DWORD channel, const GUID* context) override {
			if (!preventVolumeChange || !channelVolumeControllerIsValid.load()) {
				return S_OK;
			}

			if (recursionAtomic.load()) {
				return S_OK;
			}
			recursionAtomic.exchange(true);

			std::lock_guard<std::mutex> lock{ mut };

			bool otherNoneOne = false;
			for (index i = 0; i < index(channelCount); ++i) {
				if (i != index(channel) && volumes[i] != 1.0f) {
					otherNoneOne = true;
					// no break because we need to set everything to 1.0
					volumes[i] = 1.0f;
				}
			}

			HRESULT res = S_OK;

			if (otherNoneOne) {
				volumes[channel] = 1.0f;
				res = channelVolumeController.ref().SetAllVolumes(
					channel, volumes, nullptr
				);
			} else if (volumes[channel] != 1.0f) {
				res = channelVolumeController.ref().SetChannelVolume(
					channel, 1.0f, nullptr
				);
			}

			if (res != S_OK) {
				channelVolumeControllerIsValid.exchange(false);
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
				break;
			case DisconnectReasonFormatChanged:
				disconnectionReason.exchange(DisconnectionReason::eRECONNECT);
				break;
			case DisconnectReasonExclusiveModeOverride:
				disconnectionReason.exchange(DisconnectionReason::eEXCLUSIVE);
				break;
			}

			return S_OK;
		}
	};

	// the whole and only point of this class is to
	// automatically call AudioSessionEventsImpl#deinit
	// reasoning:
	//    As IUnknown implementation, AudioSessionEventsImpl must manage it's lifetime based on ref counter
	//    GenericComWrapper will only decrement ref counter on destruction
	//    IAudioSessionControl will still hold a reference inside itself,
	//    so the object will not be destroyed until explicit #deinit call
	class AudioSessionEventsWrapper : MovableOnlyBase {
		template<typename T>
		using GenericComWrapper = common::winapi_wrappers::GenericComWrapper<T>;

		GenericComWrapper<AudioSessionEventsImpl> impl;

	public:
		AudioSessionEventsWrapper() = default;

		void listenTo(IAudioClientWrapper& client, bool preventVolumeChange) {
			destruct();
			impl = {
				[&](auto ptr) {
					*ptr = AudioSessionEventsImpl::create(client, preventVolumeChange);
					return true;
				}
			};
		}

		void listenTo(GenericComWrapper<IAudioSessionControl>&& _sessionController, bool preventVolumeChange) {
			destruct();
			impl = {
				[&](auto ptr) {
					*ptr = AudioSessionEventsImpl::create(std::move(_sessionController), preventVolumeChange);
					return true;
				}
			};
		}

		~AudioSessionEventsWrapper() {
			destruct();
		}

		AudioSessionEventsWrapper(AudioSessionEventsWrapper&& other) noexcept {
			destruct();
			impl = std::move(other.impl);
		}

		AudioSessionEventsWrapper& operator=(AudioSessionEventsWrapper&& other) noexcept {
			if (this == &other) {
				return *this;
			}

			destruct();
			impl = std::move(other.impl);

			return *this;
		}

		[[nodiscard]]
		bool isValid() const {
			return impl.isValid();
		}

		AudioSessionEventsImpl::Changes grabChanges() {
			if (!impl.isValid()) {
				return {};
			}
			return impl.ref().takeChanges();
		}

		void destruct() {
			if (impl.isValid()) {
				impl.ref().deinit();
			}
			impl = {};
		}
	};
}
