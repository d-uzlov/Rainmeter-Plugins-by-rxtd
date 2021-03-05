// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "ProcessingOrchestrator.h"

#include "rxtd/std_fixes/MapUtils.h"

using rxtd::audio_analyzer::ProcessingOrchestrator;
using rxtd::std_fixes::MapUtils;

void ProcessingOrchestrator::reset() {
	saMap.clear();
	valid = false;
}

void ProcessingOrchestrator::patch(
	const Patches& patches,
	Version version,
	index samplesPerSec, array_view<Channel> channels
) {
	MapUtils::intersectKeyCollection(saMap, patches);
	MapUtils::intersectKeyCollection(snapshot, patches);

	for (const auto& [name, data] : patches) {
		auto& sa = saMap[name];
		sa.setParams(
			logger.context(L"{}: ", name),
			data,
			version, samplesPerSec, channels,
			snapshot[name]
		);
	}

	valid = true;
}

void ProcessingOrchestrator::configureSnapshot(Snapshot& snap) const {
	snap = snapshot;
}

void ProcessingOrchestrator::process(const ChannelMixer& channelMixer) {
	using clock = handler::HandlerBase::clock;
	using namespace std::chrono_literals;

	const auto processBeginTime = clock::now();
	const clock::time_point killTime = processBeginTime
		+ std::chrono::duration_cast<clock::duration>(1.0ms * killTimeoutMs);

	for (auto& [name, sa] : saMap) {
		sa.process(channelMixer, killTime, snapshot[name]);
	}

	if (warnTimeMs >= 0.0) {
		const auto processEndTime = clock::now();
		const auto processDuration =
			std::chrono::duration<double, std::milli>{
				processEndTime - processBeginTime
			}.count();

		if (processDuration > warnTimeMs) {
			logger.warning(
				L"processing overhead {} ms over specified {} ms",
				processDuration - warnTimeMs,
				warnTimeMs
			);
		}
	}
}

void ProcessingOrchestrator::exchangeData(Snapshot& snap) {
	std::swap(snap, snapshot);
}
