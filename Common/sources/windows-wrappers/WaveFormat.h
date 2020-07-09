/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	enum class WaveDataFormat {
		eINVALID,
		ePCM_S16,
		ePCM_F32,
	};
	struct WaveFormat {
		index samplesPerSec = 0;
		index channelsCount = 0;
		index channelMask = 0;
		WaveDataFormat format = WaveDataFormat::eINVALID;
	};
}
