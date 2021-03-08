// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <chrono>

#include "Channel.h"
#include "ChannelMixer.h"
#include "rxtd/audio_analyzer/options/ParamParser.h"
#include "rxtd/filter_utils/DownsampleHelper.h"

namespace rxtd::audio_analyzer {
	class ProcessingManager {
	public:
		using ChannelSnapshot = std::map<istring, handler::HandlerBase::Snapshot, std::less<>>;
		using Snapshot = std::map<Channel, ChannelSnapshot, std::less<>>;
		using ProcessingData = options::ProcessingData;
		using FilterCascade = filter_utils::FilterCascade;
		using DownsampleHelper = filter_utils::DownsampleHelper;

		using clock = handler::HandlerBase::clock;

		using HandlerMap = std::map<istring, std::unique_ptr<handler::HandlerBase>, std::less<>>;

		struct ChannelStruct {
			HandlerMap handlerMap;

			string filterSource;
			FilterCascade filter;
			DownsampleHelper downsampleHelper;
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
			Logger _logger,
			const ProcessingData& pd,
			Version version,
			index sampleRate, array_view<Channel> layout,
			Snapshot& snapshot
		);

		void process(const ChannelMixer& mixer, clock::time_point killTime, Snapshot& snapshot);
	};
}
