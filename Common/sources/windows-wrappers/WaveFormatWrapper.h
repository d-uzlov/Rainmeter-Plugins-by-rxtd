/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <mmeapi.h>
#include <combaseapi.h>

namespace rxu {
	class WaveFormatWrapper {
		WAVEFORMATEX *ptr = nullptr;

	public:

		WaveFormatWrapper() = default;

		WaveFormatWrapper(const WaveFormatWrapper& other) = delete;
		WaveFormatWrapper(WaveFormatWrapper&& other) = delete;
		WaveFormatWrapper& operator=(const WaveFormatWrapper& other) = delete;
		WaveFormatWrapper& operator=(WaveFormatWrapper&& other) = delete;

		~WaveFormatWrapper() {
			CoTaskMemFree(ptr);
		}

		WAVEFORMATEX** operator &() {
			return &ptr;
		}
		WAVEFORMATEX* getPointer() {
			return ptr;
		}
	};
}
