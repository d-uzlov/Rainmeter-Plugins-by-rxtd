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
#include <Audioclient.h>
#include <type_traits>

namespace rxtd::utils {
	static_assert(std::is_same<BYTE, uint8_t>::value);
	static_assert(std::is_same<UINT32, uint32_t>::value);
	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...

	class BufferWrapper {
		IAudioCaptureClient *client = nullptr;
		uint8_t* buffer = nullptr;
		uint32_t framesCount { };
		HRESULT result { };
		bool silent { };

	public:
		BufferWrapper() = default;

		BufferWrapper(IAudioCaptureClient *client) : client(client) {
			DWORD flags { };
			result = client->GetBuffer(&buffer, &framesCount, &flags, nullptr, nullptr);
			silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
		}

		BufferWrapper(BufferWrapper&& other) noexcept :
			client(other.client),
			buffer(other.buffer),
			framesCount(other.framesCount),
			result(other.result),
			silent(other.silent) {

			other.buffer = nullptr;
			other.framesCount = 0u;
			other.silent = true;
			other.result = { };
		}

		BufferWrapper& operator=(BufferWrapper&& other) noexcept {
			if (this == &other)
				return *this;

			release();

			client = other.client;
			other.client = { };
			buffer = other.buffer;
			other.buffer = { };
			framesCount = other.framesCount;
			other.framesCount = { };
			result = other.result;
			other.result = { };
			silent = other.silent;
			other.silent = { };


			return *this;
		}

		BufferWrapper(const BufferWrapper& other) = delete;
		BufferWrapper& operator=(const BufferWrapper& other) = delete;


		~BufferWrapper() {
			release();
		}

		const uint8_t* getBuffer() const {
			return buffer;
		}

		uint32_t getFramesCount() const {
			return framesCount;
		}

		bool isSilent() const {
			return silent;
		}

		HRESULT getResult() const {
			return result;
		}

	private:
		void release() {
			if (client != nullptr && buffer != nullptr) {
				client->ReleaseBuffer(framesCount);
			}
			buffer = nullptr;
			framesCount = 0u;
			silent = true;
			result = decltype(result)();
		}
	};
}
