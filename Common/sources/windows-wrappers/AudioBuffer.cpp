#include "AudioBuffer.h"
#include "IAudioCaptureClientWrapper.h"

namespace rxtd::utils {
	AudioBuffer::AudioBuffer(IAudioCaptureClientWrapper& parent, index id, bool silent, const std::byte* bufferPointer, uint32_t size) :
		parent(parent),
		id(id),
		silent(silent),
		buffer(bufferPointer, size) { }

	AudioBuffer::~AudioBuffer() {
		parent.releaseBuffer(id);
	}

}

