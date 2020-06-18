/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SilentRenderer.h"

#include "windows-wrappers/WaveFormatWrapper.h"

static constexpr long long REF_TIMES_PER_SEC = 1000'000'0; // 1 sec in 100-ns units

const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

namespace rxtd::audio_analyzer {
	SilentRenderer::SilentRenderer(IMMDevice *audioDeviceHandle) {
		if (audioDeviceHandle == nullptr) {
			return;
		}

		HRESULT hr;

		hr = audioDeviceHandle->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		utils::WaveFormatWrapper waveFormatWrapper;
		hr = audioClient->GetMixFormat(&waveFormatWrapper);
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, REF_TIMES_PER_SEC, 0, waveFormatWrapper.getPointer(), nullptr);
		if (hr != S_OK) {
			if (hr == AUDCLNT_E_DEVICE_IN_USE) {
				// exclusive mode
				releaseHandles();
				return;
			}
			releaseHandles();
			return;
		}

		static_assert(std::is_same<UINT32, uint32_t>::value);

		// get the frame count
		uint32_t bufferFramesCount;
		hr = audioClient->GetBufferSize(&bufferFramesCount);
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		// create a render client
		hr = audioClient->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&renderer));
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		static_assert(std::is_same<BYTE, uint8_t>::value);

		// get the buffer
		uint8_t *buffer;
		hr = renderer->GetBuffer(bufferFramesCount, &buffer);
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		// release it
		hr = renderer->ReleaseBuffer(bufferFramesCount, AUDCLNT_BUFFERFLAGS_SILENT);
		if (hr != S_OK) {
			releaseHandles();
			return;
		}

		// start the stream
		hr = audioClient->Start();
		if (hr != S_OK) {
			releaseHandles();
			return;
		}
	}

	SilentRenderer::SilentRenderer(SilentRenderer&& other) noexcept {
		releaseHandles();

		audioClient = other.audioClient;
		renderer = other.renderer;

		other.audioClient = nullptr;
		other.renderer = nullptr;
	}

	SilentRenderer& SilentRenderer::operator=(SilentRenderer&& other) noexcept {
		if (this == &other)
			return *this;

		releaseHandles();

		audioClient = other.audioClient;
		renderer = other.renderer;

		other.audioClient = nullptr;
		other.renderer = nullptr;

		return *this;
	}

	SilentRenderer::~SilentRenderer() {
		releaseHandles();
	}

	bool SilentRenderer::isError() const {
		return audioClient == nullptr || renderer == nullptr;
	}

	void SilentRenderer::releaseHandles() {
		if (renderer != nullptr) {
			renderer->Release();
			renderer = nullptr;
		}

		if (audioClient != nullptr) {
			audioClient->Stop();
			audioClient->Release();
			audioClient = nullptr;
		}
	}
}
