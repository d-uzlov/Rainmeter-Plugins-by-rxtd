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
#include "sound-handlers/SoundHandler.h"
#include "Vector2D.h"

namespace rxtd::audio_analyzer {
	struct ChannelData {
		std::vector<std::unique_ptr<SoundHandler>> handlers;
		std::map<istring, index, std::less<>> indexMap;
	};

	class SoundAnalyzer;
	class DataSupplierImpl : public DataSupplier {
		utils::Vector2D<float> *wave = nullptr;
		// SoundAnalyzer &parent;
		const ChannelData *channelData = nullptr;
		Channel channel { };
		index channelIndex = 0;
		index waveSize { };

		mutable index nextBufferIndex = 0;
		mutable std::vector<std::vector<std::byte>> buffers;

	public:
		explicit DataSupplierImpl(utils::Vector2D<float> &wave);
		void setChannelData(const ChannelData*);
		void setChannelIndex(index channelIndex);
		void setChannel(Channel channel);

		const float* getWave() const override;
		index getWaveSize() const override;
		const SoundHandler* getHandlerRaw(isview id) const override;
		Channel getChannel() const override;
		std::byte* getBufferRaw(index size) const override;

		void resetBuffers();
		void setWaveSize(index value);
	};
}
