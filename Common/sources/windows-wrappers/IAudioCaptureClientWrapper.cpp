#include "IAudioCaptureClientWrapper.h"
#include <type_traits>

namespace rxtd::utils {
	static_assert(std::is_same<BYTE, uint8_t>::value);
	static_assert(std::is_same<UINT32, uint32_t>::value);

	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...

	AudioBuffer IAudioCaptureClientWrapper::readBuffer() {
		releaseBuffer(lastBufferID);
		lastBufferID++;

		uint8_t* buffer = nullptr;

		DWORD flags { };
		lastResult = (*this)->GetBuffer(&buffer, &lastBufferSize, &flags, nullptr, nullptr);
		const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

		return AudioBuffer { *this, lastBufferID, silent, reinterpret_cast<const std::byte*>(buffer), lastBufferSize };
	}

	index IAudioCaptureClientWrapper::getLastResult() const {
		return lastResult;
	}

	void IAudioCaptureClientWrapper::releaseBuffer(const index id) {
		if (id != lastBufferID || lastBufferSize == 0 || !isValid()) {
			return;
		}

		(*this)->ReleaseBuffer(lastBufferSize);
		lastBufferSize = 0;
	}
}

