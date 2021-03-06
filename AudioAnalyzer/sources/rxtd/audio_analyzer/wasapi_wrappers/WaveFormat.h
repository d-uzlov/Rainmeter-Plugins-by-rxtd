// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/sound_processing/Channel.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	struct WaveFormat {
		index samplesPerSec = 0;
		index channelsCount = 0;
		ChannelLayout channelLayout{};

		// autogenerated
		friend bool operator==(const WaveFormat& lhs, const WaveFormat& rhs) {
			return lhs.samplesPerSec == rhs.samplesPerSec
				&& lhs.channelsCount == rhs.channelsCount
				&& lhs.channelLayout == rhs.channelLayout;
		}

		friend bool operator!=(const WaveFormat& lhs, const WaveFormat& rhs) {
			return !(lhs == rhs);
		}
	};

	class FormatException : public std::runtime_error {
	public:
		explicit FormatException() : runtime_error("audio format can't be parsed") {}
	};
}
