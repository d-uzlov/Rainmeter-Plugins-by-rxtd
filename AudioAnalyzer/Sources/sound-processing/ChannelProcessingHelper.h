/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <set>

#include "Channel.h"
#include "array_view.h"
#include "../audio-utils/filter-utils/FilterCascadeParser.h"
#include "../audio-utils/DownsampleHelper.h"

namespace rxtd::audio_analyzer {
	class ChannelProcessingHelper {
		struct ChannelData {
			audio_utils::FilterCascade fc;
			audio_utils::DownsampleHelper downsampleHelper;
		};

		std::map<Channel, ChannelData> channels;
		utils::GrowingVector<float> buffer;

		audio_utils::FilterCascadeCreator fcc;

		struct ResamplingData {
			index sourceRate = 0;
			index targetRate = 0;
			index finalSampleRate = 0;
			index divider = 1;
		} resamplingData;

	public:
		ChannelProcessingHelper() {
			buffer.setMaxSize(1);
		}

		// depends on both system format and options
		void setChannels(const std::set<Channel>& set);

		// depends on options only
		void setParams(audio_utils::FilterCascadeCreator _fcc, index targetRate, index sourceSampleRate);

		// depends on system format only
		void updateSourceRate(index value);

		[[nodiscard]]
		index getSampleRate() const {
			return resamplingData.finalSampleRate;
		}

		void processDataFrom(Channel channel, array_view<float> wave);

		[[nodiscard]]
		array_view<float> grabNext(index size) {
			return buffer.removeFirst(size);
		}

		[[nodiscard]]
		array_view<float> grabRest() {
			return buffer.removeFirst(buffer.getRemainingSize());
		}

	private:
		void recalculateResamplingData();
		void updateFilters();
	};
}
