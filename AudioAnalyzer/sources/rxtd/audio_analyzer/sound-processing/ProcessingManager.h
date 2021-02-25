/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <chrono>

#include "Channel.h"
#include "ChannelMixer.h"
#include "rxtd/audio_analyzer/audio-utils/DownsampleHelper.h"
#include "rxtd/audio_analyzer/options/ParamParser.h"

namespace rxtd::audio_analyzer {
	class ProcessingManager {
	public:
		using Logger = common::rainmeter::Logger;
		using ChannelSnapshot = std::map<istring, handler::HandlerBase::Snapshot, std::less<>>;
		using Snapshot = std::map<Channel, ChannelSnapshot, std::less<>>;

		using clock = handler::HandlerBase::clock;

		using HandlerMap = std::map<istring, std::unique_ptr<handler::HandlerBase>, std::less<>>;

		struct ChannelStruct {
			HandlerMap handlerMap;

			audio_utils::FilterCascade filter;
			audio_utils::DownsampleHelper downsampleHelper;
		};

		class HandlerFinderImpl : public HandlerFinder {
			const HandlerMap& channelData;

		public:
			explicit HandlerFinderImpl(const HandlerMap& channelData) : channelData(channelData) { }

			[[nodiscard]]
			handler::HandlerBase* getHandler(isview id) const override {
				const auto iter = channelData.find(id);
				return iter == channelData.end() ? nullptr : iter->second.get();
			}
		};

	private:
		Logger logger;
		std::vector<istring> order;
		std::map<Channel, ChannelStruct> channelMap;
		index resamplingDivider{};
		std::vector<float> downsampledBuffer;
		std::vector<float> filteredBuffer;

	public:
		void setParams(
			Logger logger,
			const ProcessingData& pd,
			Version version,
			index sampleRate, array_view<Channel> layout,
			Snapshot& snapshot
		);

		void process(const ChannelMixer& mixer, clock::time_point killTime, Snapshot& snapshot);
	};
}
