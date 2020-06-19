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
