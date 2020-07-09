#include "AudioBuffer.h"
#include "IAudioCaptureClientWrapper.h"

namespace rxtd::utils {
	AudioBuffer::AudioBuffer(IAudioCaptureClientWrapper& parent, index id, bool silent, array_view<std::byte> buffer) :
		parent(parent),
		id(id),
		silent(silent),
		buffer(buffer) { }

	AudioBuffer::~AudioBuffer() {
		parent.releaseBuffer(id);
	}
}

