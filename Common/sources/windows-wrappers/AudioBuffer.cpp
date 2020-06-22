#include "AudioBuffer.h"
#include "IAudioCaptureClientWrapper.h"

namespace rxtd::utils {
	AudioBuffer::AudioBuffer(IAudioCaptureClientWrapper& parent, index id, bool silent, const uint8_t* buffer, uint32_t size) :
		parent(parent),
		id(id),
		silent(silent),
		buffer(buffer),
		size(size) { }

	AudioBuffer::~AudioBuffer() {
		parent.releaseBuffer(id);
	}

}

