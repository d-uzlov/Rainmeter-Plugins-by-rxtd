/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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