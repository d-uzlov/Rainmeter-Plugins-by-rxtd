/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "ProcessingManager.h"

namespace rxtd::audio_analyzer {
	class ProcessingOrchestrator {
		struct {
			index samplesPerSec{ };
			ChannelLayout channelLayout;
		} currentFormat;

		double computeTimeoutMs = 33.0;
		double killTimeoutMs = 33.0;

		utils::Rainmeter::Logger logger;

		std::map<istring, ProcessingManager, std::less<>> saMap;

	public:
		void setLogger(utils::Rainmeter::Logger value) {
			logger = std::move(value);
		}

		void setKillTimeout(double value) {
			killTimeoutMs = value;
		}
		
		void setComputeTimeout(double value) {
			computeTimeoutMs = value;
		}
		
		void setFormat(index samplesPerSec, ChannelLayout channelLayout);

		void patch(const ParamParser::ProcessingsInfoMap& patches, index legacyNumber);

		void process(const ChannelMixer& channelMixer);

		bool getProp(isview proc, isview id, Channel channel, isview propName, utils::BufferPrinter& printer);

		double temp_getValue(isview proc, isview id, Channel channel, index ind) const;
	};
}
