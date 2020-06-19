#include "IAudioCaptureClientWrapper.h"
#include <type_traits>

namespace rxtd::utils {
	static_assert(std::is_same<BYTE, uint8_t>::value);
	static_assert(std::is_same<UINT32, uint32_t>::value);

	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...


	IAudioCaptureClientWrapper::~IAudioCaptureClientWrapper() {
		releaseBuffer();
	}

	AudioBuffer IAudioCaptureClientWrapper::readBuffer() {

		releaseBuffer();

		uint8_t* buffer = nullptr;
		uint32_t framesCount { };

		DWORD flags { };
		lastResult = (*this)->GetBuffer(&buffer, &framesCount, &flags, nullptr, nullptr);
		const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

		lastBuffer = { buffer, framesCount, silent };

		return lastBuffer;
	}

	index IAudioCaptureClientWrapper::getLastResult() const {
		return lastResult;
	}

	void IAudioCaptureClientWrapper::releaseBuffer() {
		if (!isValid()) {
			return;
		}

		(*this)->ReleaseBuffer(lastBuffer.framesCount);
		lastBuffer = { };
	}
}

