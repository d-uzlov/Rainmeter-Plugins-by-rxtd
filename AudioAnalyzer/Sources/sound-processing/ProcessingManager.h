/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <chrono>

#include "Channel.h"
#include "../ParamParser.h"
#include "ChannelMixer.h"
#include "../audio-utils/DownsampleHelper.h"

namespace rxtd::audio_analyzer {
	class ProcessingManager {
	public:
		using ChannelSnapshot = std::map<istring, SoundHandler::Snapshot, std::less<>>;
		using Snapshot = std::map<Channel, ChannelSnapshot, std::less<>>;

	private:
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		using HandlerMap = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;

		struct ChannelStruct {
			HandlerMap handlerMap;

			audio_utils::FilterCascade fc;
			audio_utils::DownsampleHelper downsampleHelper;
		};

		std::vector<istring> order;

		std::map<Channel, ChannelStruct> channelMap;

		class HandlerFinderImpl : public HandlerFinder {
			const HandlerMap& channelData;

		public:
			explicit HandlerFinderImpl(const HandlerMap& channelData) : channelData(channelData) {
			}

			[[nodiscard]]
			SoundHandler* getHandler(isview id) const override {
				const auto iter = channelData.find(id);
				return iter == channelData.end() ? nullptr : iter->second.get();
			}
		};

		struct ResamplingData {
			index sourceRate = 0;
			index targetRate = 0;
			index finalSampleRate = 0;
			index divider = 1;

			void setParams(index _sourceRate, index _targetRate) {
				sourceRate = _sourceRate;
				targetRate = _targetRate;

				if (targetRate == 0) {
					divider = 1;
				} else {
					const auto ratio = static_cast<double>(sourceRate) / targetRate;
					divider = ratio > 1 ? static_cast<index>(ratio) : 1;
				}
				finalSampleRate = sourceRate / divider;
			}
		} resamplingData;

		std::vector<float> waveBuffer;

	public:
		void setParams(
			utils::Rainmeter::Logger logger,
			const ParamParser::ProcessingData& pd,
			index _legacyNumber,
			index sampleRate, ChannelLayout layout
		);

		// returns true when killed on timeout
		bool process(const ChannelMixer& mixer, clock::time_point killTime);
		void configureSnapshot(Snapshot& snapshot);
		void updateSnapshot(Snapshot& snapshot);
	};
}
