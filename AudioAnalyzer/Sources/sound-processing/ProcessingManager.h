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
#include "ChannelProcessingHelper.h"
#include "../ParamParser.h"
#include "ChannelMixer.h"

namespace rxtd::audio_analyzer {
	class ProcessingManager {
	public:
		using ChannelSnapshot = std::map<istring, SoundHandler::Snapshot, std::less<>>;
		using Snapshot = std::map<Channel, ChannelSnapshot, std::less<>>;

	private:
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		using ChannelData = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;
		using Logger = utils::Rainmeter::Logger;

		std::vector<istring> order;

		std::map<Channel, ChannelData> channelMap;

		ChannelProcessingHelper cph;

		index legacyNumber = 0;

		class HandlerFinderImpl : public HandlerFinder {
			const ChannelData& channelData;

		public:
			explicit HandlerFinderImpl(const ChannelData& channelData) : channelData(channelData) { }

			[[nodiscard]]
			SoundHandler* getHandler(isview id) const override {
				const auto iter = channelData.find(id);
				return iter == channelData.end() ? nullptr : iter->second.get();
			}
		};

	public:
		void updateSnapshot(Snapshot& snapshot);

		void setParams(
			Logger logger,
			const ParamParser::ProcessingData& pd,
			index _legacyNumber,
			index sampleRate, ChannelLayout layout
		);

		// returns true when killed on timeout
		bool process(const ChannelMixer& mixer, clock::time_point killTime);
		void configureSnapshot(Snapshot& snapshot);
	};
}
