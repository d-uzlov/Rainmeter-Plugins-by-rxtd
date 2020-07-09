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
#include "IAudioCaptureClientWrapper.h"
#include "WaveFormat.h"
#include "MediaDeviceType.h"

namespace rxtd::utils {

	class IAudioClientWrapper : public GenericComWrapper<IAudioClient> {
		index lastResult = { };
		WaveFormat format;
		MediaDeviceType type { };

	public:
		IAudioClientWrapper() = default;
		explicit IAudioClientWrapper(MediaDeviceType type, InitFunction initFunction);

		IAudioCaptureClientWrapper openCapture();

		WaveFormat getFormat() const;
		void initShared();

		index getLastResult() const;
	};
}
