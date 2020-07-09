/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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
		IAudioCaptureClientWrapper() = default;
		explicit IAudioCaptureClientWrapper(InitFunction initFunction);

		// Be careful, call to this function invalidates all previous buffers
		AudioBuffer readBuffer();

		index getLastResult() const;

	private:
		void releaseBuffer(index id);
	};
}
