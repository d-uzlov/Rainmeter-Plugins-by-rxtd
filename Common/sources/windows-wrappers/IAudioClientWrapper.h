#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>
#include "IAudioCaptureClientWrapper.h"
#include "WaveFormat.h"

namespace rxtd::utils {

	class IAudioClientWrapper : public GenericComWrapper<IAudioClient> {
	private:
		index lastResult = { };
		WaveFormat format;

	public:
		IAudioCaptureClientWrapper openCapture();

		WaveFormat getFormat() const;
		void initShared(bool loopback);

		index getLastResult() const;
	};
}
