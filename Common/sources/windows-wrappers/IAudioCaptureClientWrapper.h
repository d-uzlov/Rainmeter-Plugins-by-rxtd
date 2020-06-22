#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>
#include "AudioBuffer.h"

namespace rxtd::utils {
	class IAudioCaptureClientWrapper : public GenericComWrapper<IAudioCaptureClient> {
		friend AudioBuffer;

		index lastBufferID = 0;
		uint32_t lastBufferSize { };
		index lastResult { };


	public:
		// Be careful, call to this function invalidates all previous buffers
		AudioBuffer readBuffer();

		index getLastResult() const;

	private:
		void releaseBuffer(index id);
	};
}
