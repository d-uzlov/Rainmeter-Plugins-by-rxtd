/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "device-management/MyWaveFormat.h"
#include "array_view.h"
#include "GrowingVector.h"
#include "array2d_view.h"

namespace rxtd::audio_analyzer {
	class ChannelMixer {
		MyWaveFormat waveFormat;
		std::map<Channel, utils::GrowingVector<float>> channels;
		Channel aliasOfAuto = Channel::eAUTO;

	public:
		void setFormat(MyWaveFormat waveFormat);
		void saveChannelsData(utils::array2d_view<float> channelsData, bool withAuto);
		void writeSilence(index size, bool withAuto);

		[[nodiscard]]
		array_view<float> getChannelPCM(Channel channel) const;

		void reset() {
			for (auto& [channel, buffer] : channels) {
				buffer.reset();
			}
		}

	private:
		void resampleToAuto(index size);
	};
}
