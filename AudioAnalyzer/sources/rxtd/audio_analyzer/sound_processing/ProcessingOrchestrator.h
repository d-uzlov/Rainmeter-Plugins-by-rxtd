// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "ProcessingManager.h"

namespace rxtd::audio_analyzer {
	class ProcessingOrchestrator {
	public:
		using Snapshot = std::map<istring, ProcessingManager::Snapshot, std::less<>>;
		using Patches = options::ParamHelper::ProcessingsInfoMap;

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
			const Patches& patches,
			Version version,
			index samplesPerSec, array_view<Channel> channels
		);
		void configureSnapshot(Snapshot& snap) const;

		void process(const ChannelMixer& channelMixer);
		void exchangeData(Snapshot& snap);
	};
}
