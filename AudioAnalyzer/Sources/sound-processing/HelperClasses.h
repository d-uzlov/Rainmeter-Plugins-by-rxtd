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

namespace rxtd::audio_analyzer {
	struct ChannelData {
		std::vector<std::unique_ptr<SoundHandler>> handlers;
		std::map<istring, index, std::less<>> indexMap;
	};

	class DataSupplierImpl : public DataSupplier {
		array_view<float> wave{ };
		const ChannelData* channelData = nullptr;
		index waveSize{ };

		mutable index nextBufferIndex = 0;
		mutable std::vector<std::vector<std::byte>> buffers;

	public:
		void setWave(array_view<float> wave);
		void setChannelData(const ChannelData* channelData);

		array_view<float> getWave() const override;
		std::byte* getBufferRaw(index size) const override;

		void resetBuffers();
		void setWaveSize(index value);

	protected:
		const SoundHandler* getHandlerRaw(isview id) const override;
	};
}
