/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <cstdint>
#include <Audioclient.h>
#include <type_traits>

#pragma warning(disable : 6215)
#pragma warning(disable : 6217)

namespace rxu {
	static_assert(std::is_same<BYTE, uint8_t>::value);
	static_assert(std::is_same<UINT32, uint32_t>::value);
	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...

	class BufferWrapper {
		IAudioCaptureClient *const client;
		uint8_t* buffer = nullptr;
		uint32_t framesCount {};
		HRESULT result {};
		bool silent {};

	public:

		BufferWrapper() : client(nullptr) {
		}

		BufferWrapper(IAudioCaptureClient *client) : client(client) {
			DWORD flags {};
			result = client->GetBuffer(&buffer, &framesCount, &flags, nullptr, nullptr);
			silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
		}

		BufferWrapper(BufferWrapper&& other) noexcept
			: client(other.client),
			  buffer(other.buffer),
			  framesCount(other.framesCount),
			  result(other.result),
			  silent(other.silent) {
			other.buffer = nullptr;
			other.framesCount = 0u;
			other.silent = true;
			other.result = { };
		}

		BufferWrapper& operator=(BufferWrapper&& other) noexcept = delete;
		BufferWrapper(const BufferWrapper& other) = delete;
		BufferWrapper& operator=(const BufferWrapper& other) = delete;


		~BufferWrapper() {
			if (client != nullptr && buffer != nullptr) {
				client->ReleaseBuffer(framesCount);
			}
			buffer = nullptr;
			framesCount = 0u;
			silent = true;
			result = decltype(result)();
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
	};
}
