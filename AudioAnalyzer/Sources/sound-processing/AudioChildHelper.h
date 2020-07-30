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
#include "HelperClasses.h"

namespace rxtd::audio_analyzer {
	class AudioChildHelper {
		const std::map<Channel, ChannelData>* channels;

	public:
		AudioChildHelper() = default;
		explicit AudioChildHelper(const std::map<Channel, ChannelData>& channels);

		[[nodiscard]]
		SoundHandler* findHandler(Channel channel, isview handlerId) const;
		[[nodiscard]]
		double getValue(Channel channel, isview handlerId, index index) const;
		[[nodiscard]]
		double getValueFrom(SoundHandler* handler, Channel channel, index index) const;
	};
}
