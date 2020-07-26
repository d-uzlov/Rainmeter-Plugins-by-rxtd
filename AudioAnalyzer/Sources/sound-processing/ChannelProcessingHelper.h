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
#include "Resampler.h"
#include "array_view.h"
#include "../audio-utils/filter-utils/FilterCascadeParser.h"
#include "ChannelMixer.h"

namespace rxtd::audio_analyzer {
	class ChannelProcessingHelper {
		struct ChannelData {
			std::vector<float> wave;
			bool preprocessed = false;
		};

		MyWaveFormat waveFormat;
		mutable std::map<Channel, ChannelData> channels;
		Resampler resampler;

		const ChannelMixer* mixer{ };

		audio_utils::FilterCascadeCreator fcc;
		audio_utils::FilterCascade fc;

	public:
		void setChannelMixer(const ChannelMixer& value) {
			mixer = &value;
		}

		void setFCC(audio_utils::FilterCascadeCreator value);

		void setSampleRate(index value) {
			fc = fcc.getInstance(value);
			resampler.setSourceRate(value);
		}

		array_view<float> getChannelPCM(Channel channel) const;

		Resampler& getResampler() {
			return resampler;
		}

		const Resampler& getResampler() const {
			return resampler;
		}

		void reset() const;
	};
}
