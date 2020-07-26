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

namespace rxtd::audio_analyzer {
	class ChannelMixer {
		struct ChannelData {
			std::vector<float> wave;
			bool preprocessed = false;
		};

		MyWaveFormat waveFormat;
		mutable std::map<Channel, ChannelData> channels;
		Channel aliasOfAuto = Channel::eAUTO;
		Resampler resampler;
		audio_utils::FilterCascadeCreator fcc;
		audio_utils::FilterCascade fc;
		index waveSize{ };

	public:
		void setFormat(MyWaveFormat waveFormat);
		void setFCC(audio_utils::FilterCascadeCreator value);
		void decomposeFramesIntoChannels(array_view<std::byte> frameBuffer, bool withAuto);

		array_view<float> getChannelPCM(Channel channel) const;

		Resampler& getResampler() {
			return resampler;
		}

		const Resampler& getResampler() const {
			return resampler;
		}

		index getWaveSize() const {
			return waveSize;
		}

	private:
		void resampleToAuto();
	};
}
