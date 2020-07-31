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

namespace rxtd::audio_analyzer {
	class ChannelProcessingHelper {
		struct ChannelData {
			utils::GrowingVector<float> wave;
			audio_utils::FilterCascade fc;
			index decimationCounter = 0;
			// Resampler resampler;
			bool preprocessed = false;
		};

		MyWaveFormat waveFormat;
		mutable std::map<Channel, ChannelData> channels;

		const ChannelMixer* mixer{ };

		audio_utils::FilterCascadeCreator fcc;

		struct ResamplingData {
			index sourceRate = 0;
			index targetRate = 0;
			index finalSampleRate = 0;
			index divider = 1;
		} resamplingData;

		Channel currentChannel{ };
		index grabBufferSize = 0;

	public:
		ChannelProcessingHelper() = default;

		void setChannelMixer(const ChannelMixer& value) {
			mixer = &value;
		}

		// depends on both system format and options
		void setChannels(const std::set<Channel>& set);

		// depends on options only
		void setParams(audio_utils::FilterCascadeCreator _fcc, index targetRate);

		// depends on both system format only
		void setSourceRate(index value);

		[[nodiscard]]
		index getSampleRate() const {
			return resamplingData.finalSampleRate;
		}

		void setGrabBufferSize(index value) {
			grabBufferSize = value;
		}

		void setCurrentChannel(Channel value) {
			if (mixer == nullptr) {
				return;
			}

			if (value == Channel::eAUTO) {
				value = mixer->getAutoAlias();
			}

			currentChannel = value;
		}

		[[nodiscard]]
		array_view<float> grabNext() const {
			cacheChannel();
			return channels[currentChannel].wave.takeChunk(grabBufferSize);
		}

		void reset() const;

	private:
		void cacheChannel() const;

		void recalculateResamplingData();
		void updateFC();
	};
}
