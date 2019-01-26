/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <cstdint>
#include <vector>
#include "MyWaveFormat.h"
#include "sound-handlers/SoundHandler.h"
#include <functional>
#include <map>
#include "Channel.h"
#include <ContinuousBuffersHolder.h>
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

		std::map<Channel, std::vector<std::wstring>> orderOfHandlers;
		std::map<std::wstring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> patchers;

		unsigned targetRate = 0u;
		unsigned divide = 1u;

		rxu::ContinuousBuffersHolder<float> wave;

		struct ChannelData {
			std::vector<std::unique_ptr<SoundHandler>> handlers;
			std::map<std::wstring, decltype(handlers)::size_type> indexMap;
		};

		std::map<Channel, ChannelData> channels;

		class DataSupplierImpl : public DataSupplier {
			SoundAnalyzer &parent;
			ChannelData *channelData = nullptr;
			Channel channel { };
			unsigned channelIndex = 0;
			unsigned waveSize { };

			mutable unsigned nextBufferIndex = 0;
			mutable std::vector<std::vector<uint8_t>> buffers;

		public:
			explicit DataSupplierImpl(SoundAnalyzer& parent);
			void setChannelData(ChannelData*);
			void setChannelIndex(unsigned channelIndex);
			void setChannel(Channel channel);

			const float* getWave() const override;
			unsigned getWaveSize() const override;
			const SoundHandler* getHandler(const std::wstring& id) const override;
			Channel getChannel() const override;
			uint8_t* getBufferRaw(size_t size) const override;

			void resetBuffers();
			void setWaveSize(unsigned value);
		} dataSupplier;

	public:

		SoundAnalyzer() noexcept;

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = delete;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = delete;

		void setTargetRate(unsigned value) noexcept;

		std::variant<SoundHandler*, SearchError> findHandler(Channel channel, const std::wstring& handlerId) const noexcept;

		double getValue(Channel channel, const std::wstring& handlerId, unsigned index) const noexcept;
		std::variant<const wchar_t*, SearchError> getProp(Channel channel, const std::wstring& handlerId, const std::wstring_view& prop) const noexcept;

		void setPatchHandlers(std::map<Channel, std::vector<std::wstring>> handlersOrder, std::map<std::wstring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap) noexcept;

		void setWaveFormat(MyWaveFormat waveFormat) noexcept;

		void process(const uint8_t* buffer, bool isSilent, uint32_t framesCount) noexcept;
		void resetValues() noexcept;

	private:
		void decompose(const uint8_t* buffer, uint32_t framesCount) noexcept;
		void resample(float* values, unsigned framesCount) const noexcept;
		void updateSampleRate() noexcept;
		int createChannelAuto(uint32_t framesCount) noexcept;
		void resampleToAuto(unsigned first, unsigned second, uint32_t framesCount) noexcept;
		void copyToAuto(unsigned index, uint32_t framesCount) noexcept;
	};

}
