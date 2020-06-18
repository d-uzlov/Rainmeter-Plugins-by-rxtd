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
#include "Vector2D.h"

namespace rxtd::audio_analyzer {
	class ChannelMixer {
	private:
		ChannelLayout layout;
		utils::Vector2D<float> *bufferPtr = nullptr;

	public:
		void setLayout(ChannelLayout layout);
		void setBuffer(utils::Vector2D<float> &buffer);

		index createChannelAuto(index framesCount, index destinationChannel);

	private:
		void resampleToAuto(index first, index second, index framesCount, index destinationChannel);
		void copyToAuto(index channelIndex, index framesCount, index destinationChannel);
	};

}
