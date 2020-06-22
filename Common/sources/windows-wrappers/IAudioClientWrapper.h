#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>
#include "IAudioCaptureClientWrapper.h"
#include "WaveFormat.h"
#include "MediaDeviceType.h"

namespace rxtd::utils {

	class IAudioClientWrapper : public GenericComWrapper<IAudioClient> {
	private:
		index lastResult = { };
		WaveFormat format;
		MediaDeviceType type { };

	public:
		IAudioClientWrapper() = default;
		explicit IAudioClientWrapper(MediaDeviceType type);

		IAudioCaptureClientWrapper openCapture();

		WaveFormat getFormat() const;
		void initShared();

		index getLastResult() const;
	};
}
