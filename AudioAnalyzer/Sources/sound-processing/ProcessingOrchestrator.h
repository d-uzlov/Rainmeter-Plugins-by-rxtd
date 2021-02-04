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
	public:
		using Logger = ::rxtd::common::rainmeter::Logger;
		using Snapshot = std::map<istring, ProcessingManager::Snapshot, std::less<>>;

	private:
		double warnTimeMs = 33.0;
		double killTimeoutMs = 33.0;

		Logger logger;

		std::map<istring, ProcessingManager, std::less<>> saMap;
		Snapshot snapshot;

		bool valid = false;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void setKillTimeout(double value) {
			killTimeoutMs = value;
		}

		void setWarnTime(double value) {
			warnTimeMs = value;
		}

		[[nodiscard]]
		bool isValid() const {
			return valid;
		}

		void reset();

		void patch(
			const ParamParser::ProcessingsInfoMap& patches,
			Version version,
			index samplesPerSec, ChannelLayout channelLayout
		);
		void configureSnapshot(Snapshot& snap) const;

		void process(const ChannelMixer& channelMixer);
		void exchangeData(Snapshot& snap);
	};
}
