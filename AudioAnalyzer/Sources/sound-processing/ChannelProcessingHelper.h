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
#include "../audio-utils/filter-utils/FilterCascadeParser.h"
#include "ChannelMixer.h"
#include <set>
#include "../audio-utils/DownsampleHelper.h"

namespace rxtd::audio_analyzer {
	class ChannelProcessingHelper {
		struct ChannelData {
			utils::GrowingVector<float> wave;
			audio_utils::FilterCascade fc;
			audio_utils::DownsampleHelper<10> downsampleHelper;
		};

		MyWaveFormat waveFormat;
		std::map<Channel, ChannelData> channels;

		audio_utils::FilterCascadeCreator fcc;

		struct ResamplingData {
			index sourceRate = 0;
			index targetRate = 0;
			index finalSampleRate = 0;
			index divider = 1;
		} resamplingData;

		Channel currentChannel{ };
		Channel autoAlias{ };
		index grabBufferSize = 0;

	public:
		ChannelProcessingHelper() = default;

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

		void setGrabBufferSize(index value) {
			grabBufferSize = value;
		}

		void setCurrentChannel(Channel value) {
			currentChannel = value == Channel::eAUTO ? autoAlias : value;
		}

		void processDataFrom(const ChannelMixer& mixer);

		[[nodiscard]]
		array_view<float> grabNext() {
			return channels[currentChannel].wave.removeFirst(grabBufferSize);
		}

		[[nodiscard]]
		array_view<float> grabRest() {
			auto& waveBuffer = channels[currentChannel].wave;
			return waveBuffer.removeFirst(waveBuffer.getRemainingSize());
		}

		void reset();

	private:
		void processChannel(Channel channel, const ChannelMixer& mixer);

		void recalculateResamplingData();
		void updateFilters();
	};
}
