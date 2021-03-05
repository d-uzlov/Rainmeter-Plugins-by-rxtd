// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "Channel.h"
#include "rxtd/GrowingVector.h"
#include "rxtd/std_fixes/Vector2D.h"

namespace rxtd::audio_analyzer {
	class ChannelMixer {
		ChannelLayout layout;
		std::map<Channel, GrowingVector<float>> channels;
		Channel aliasOfAuto = Channel::eAUTO;

	public:
		void setLayout(const ChannelLayout& _layout);

		void saveChannelsData(std_fixes::array2d_view<float> channelsData);
		void createAuto();

		[[nodiscard]]
		array_view<float> getChannelPCM(Channel channel) const;

		void reset() {
			for (auto& [channel, buffer] : channels) {
				buffer.reset();
			}
		}
	};
}
