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

// my-windows must be before any WINAPI include
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <audiopolicy.h>

#include "rxtd/audio_analyzer/wasapi_wrappers/AudioClientHandle.h"
#include "rxtd/winapi_wrappers/GenericComWrapper.h"
#include "rxtd/winapi_wrappers/IUnknownImpl.h"


namespace rxtd::audio_analyzer::wasapi_wrappers::implementations {
	class AudioSessionEventsImpl : public winapi_wrappers::IUnknownImpl<IAudioSessionEvents> {
	public:
		template<typename T>
		using GenericComWrapper = winapi_wrappers::GenericComWrapper<T>;

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
		static const GUID guid;

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

		std::mutex mut;

	public:
		static AudioSessionEventsImpl* create(AudioClientHandle& audioClient, bool preventVolumeChange) noexcept(false);

		static AudioSessionEventsImpl* create(GenericComWrapper<IAudioSessionControl>&& _sessionController, bool preventVolumeChange) noexcept(false);

	private:
		AudioSessionEventsImpl() = default;

	protected:
		virtual ~AudioSessionEventsImpl() = default;

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
		void deinit();

		[[nodiscard]]
		Changes takeChanges() {
			Changes result{};
			result.disconnectionReason = disconnectionReason.exchange(DisconnectionReason::eNONE);
			return result;
		}

		HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(const wchar_t* name, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnIconPathChanged(const wchar_t* iconPath, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float volumeLevel, BOOL isMuted, const GUID* context) override;

		HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD channelCount, float volumes[], DWORD channel, const GUID* context) override;

		HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(const GUID* groupingParam, const GUID* context) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState state) override {
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason reason) override;
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
		using GenericComWrapper = winapi_wrappers::GenericComWrapper<T>;

		GenericComWrapper<AudioSessionEventsImpl> impl;

	public:
		AudioSessionEventsWrapper() = default;

		void listenTo(AudioClientHandle& client, bool preventVolumeChange) {
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
