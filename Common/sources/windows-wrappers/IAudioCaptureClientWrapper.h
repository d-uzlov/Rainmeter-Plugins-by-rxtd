#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>
#include "AudioBuffer.h"

namespace rxtd::utils {
	class IAudioCaptureClientWrapper : public GenericComWrapper<IAudioCaptureClient> {
	private:
		AudioBuffer lastBuffer { };
		index lastResult = { };

	public:
		IAudioCaptureClientWrapper() = default;
		IAudioCaptureClientWrapper(IAudioCaptureClientWrapper&& other) noexcept = default;
		IAudioCaptureClientWrapper& operator=(IAudioCaptureClientWrapper&& other) noexcept = default;
		IAudioCaptureClientWrapper(const IAudioCaptureClientWrapper& other) = delete;
		IAudioCaptureClientWrapper& operator=(const IAudioCaptureClientWrapper& other) = delete;
		~IAudioCaptureClientWrapper();

		AudioBuffer readBuffer();

		index getLastResult() const;

		void releaseBuffer();
	};
}
