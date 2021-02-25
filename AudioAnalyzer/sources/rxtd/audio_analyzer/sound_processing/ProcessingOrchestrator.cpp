/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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
			logger.context(L"Proc '{}': ", name),
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
