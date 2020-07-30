/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IAudioCaptureClientWrapper.h"
#include <type_traits>

namespace rxtd::utils {
	static_assert(std::is_same<BYTE, uint8_t>::value);
	static_assert(std::is_same<UINT32, uint32_t>::value);

	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...

	IAudioCaptureClientWrapper::IAudioCaptureClientWrapper(InitFunction initFunction) :
		GenericComWrapper(std::move(initFunction)) {
	}

	AudioBuffer IAudioCaptureClientWrapper::readBuffer() {
		releaseBuffer(lastBufferID);
		lastBufferID++;

		uint8_t* bufferData = nullptr;

		DWORD flags{ };
		lastResult = getPointer()->GetBuffer(&bufferData, &lastBufferSize, &flags, nullptr, nullptr);
		const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

		// TODO this is incorrect
		// lastBufferSize is in frames but array_view operates std::byte
		const array_view<std::byte> buffer = { reinterpret_cast<const std::byte*>(bufferData), lastBufferSize };

		return AudioBuffer{ *this, lastBufferID, silent, buffer };
	}

	index IAudioCaptureClientWrapper::getLastResult() const {
		return lastResult;
	}

	void IAudioCaptureClientWrapper::releaseBuffer(const index id) {
		if (id != lastBufferID || lastBufferSize == 0 || !isValid()) {
			return;
		}

		getPointer()->ReleaseBuffer(lastBufferSize);
		lastBufferSize = 0;
	}
}
