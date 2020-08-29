/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ProcessingOrchestrator.h"

#include "MapUtils.h"

using namespace audio_analyzer;

void ProcessingOrchestrator::reset() {
	saMap.clear();
}

void ProcessingOrchestrator::patch(
	const ParamParser::ProcessingsInfoMap& patches,
	index legacyNumber,
	index samplesPerSec, ChannelLayout channelLayout
) {
	utils::MapUtils::intersectKeyCollection(saMap, patches);
	utils::MapUtils::intersectKeyCollection(snapshot, patches);

	for (const auto& [name, data] : patches) {
		auto& sa = saMap[name];
		sa.setParams(
			logger.context(L"Proc '{}': ", name),
			data,
			legacyNumber, samplesPerSec, channelLayout,
			snapshot[name]
		);
	}
}

void ProcessingOrchestrator::configureSnapshot(Snapshot& snap) const {
	snap = snapshot;
}

void ProcessingOrchestrator::process(const ChannelMixer& channelMixer) {
	using clock = SoundHandler::clock;
	using namespace std::chrono_literals;

	const auto processBeginTime = clock::now();
	const clock::time_point killTime = processBeginTime + std::chrono::duration_cast<clock::duration>(
		1.0ms * killTimeoutMs);

	for (auto& [name, sa] : saMap) {
		sa.process(channelMixer, killTime);
	}

	if (warnTimeMs >= 0.0) {
		const auto processEndTime = clock::now();
		const auto processDuration =
			std::chrono::duration<double, std::milli>{
				processEndTime - processBeginTime
			}.count();

		if (processDuration > warnTimeMs) {
			logger.debug(
				L"processing overhead {} ms over specified {} ms",
				processDuration - warnTimeMs,
				warnTimeMs
			);
		}
	}

	for (auto& [name, sa] : saMap) {
		sa.updateSnapshot(snapshot[name]);
	}
}

void ProcessingOrchestrator::exchangeData(Snapshot& snap) {
	std::swap(snap, snapshot);
}
