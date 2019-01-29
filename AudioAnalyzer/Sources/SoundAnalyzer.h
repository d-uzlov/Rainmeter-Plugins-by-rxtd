/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "MyWaveFormat.h"
#include "sound-handlers/SoundHandler.h"
#include <functional>
#include "Channel.h"
#include <Vector2D.h>
#include <variant>

namespace rxaa {
	class SoundAnalyzer {
	public:
		enum class SearchError {
			CHANNEL_NOT_FOUND,
			HANDLER_NOT_FOUND,
		};
	private:
		MyWaveFormat waveFormat;

		std::map<Channel, std::vector<istring>> orderOfHandlers;
		std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> patchers;

		index targetRate = 0u;
		index divide = 1u;

		utils::Vector2D<float> wave;

		struct ChannelData {
			std::vector<std::unique_ptr<SoundHandler>> handlers;
			std::map<istring, index, std::less<>> indexMap;
		};

		std::map<Channel, ChannelData> channels;

		class DataSupplierImpl : public DataSupplier {
			SoundAnalyzer &parent;
			ChannelData *channelData = nullptr;
			Channel channel { };
			index channelIndex = 0;
			index waveSize { };

			mutable index nextBufferIndex = 0;
			mutable std::vector<std::vector<uint8_t>> buffers;

		public:
			explicit DataSupplierImpl(SoundAnalyzer& parent);
			void setChannelData(ChannelData*);
			void setChannelIndex(index channelIndex);
			void setChannel(Channel channel);

			const float* getWave() const override;
			index getWaveSize() const override;
			const SoundHandler* getHandler(isview id) const override;
			Channel getChannel() const override;
			uint8_t* getBufferRaw(index size) const override;

			void resetBuffers();
			void setWaveSize(index value);
		} dataSupplier;

	public:

		SoundAnalyzer() noexcept;

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = delete;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = delete;

		void setTargetRate(index value) noexcept;

		std::variant<SoundHandler*, SearchError> findHandler(Channel channel, isview handlerId) const noexcept;

		double getValue(Channel channel, isview handlerId, index index) const noexcept;
		std::variant<const wchar_t*, SearchError> getProp(Channel channel, sview handlerId, sview prop) const noexcept;

		void setPatchHandlers(std::map<Channel, std::vector<istring>> handlersOrder, std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap) noexcept;

		void setWaveFormat(MyWaveFormat waveFormat) noexcept;

		void process(const uint8_t* buffer, bool isSilent, index framesCount) noexcept;
		void resetValues() noexcept;
		void finish() noexcept;

	private:
		void decompose(const uint8_t* buffer, index framesCount) noexcept;
		void resample(float* values, index framesCount) const noexcept;
		void updateSampleRate() noexcept;
		index createChannelAuto(index framesCount) noexcept;
		void resampleToAuto(index first, index second, index framesCount) noexcept;
		void copyToAuto(index channelIndex, index framesCount) noexcept;
	};

}
