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

void ProcessingOrchestrator::exchangeData(DataSnapshot& snapshot) {
	std::swap(snapshot, dataSnapshot);
}

void ProcessingOrchestrator::patch(
	const ParamParser::ProcessingsInfoMap& patches,
	index legacyNumber,
	index samplesPerSec, ChannelLayout channelLayout
) {
	utils::MapUtils::intersectKeyCollection(saMap, patches);
	utils::MapUtils::intersectKeyCollection(dataSnapshot, patches);

	for (const auto& [name, data] : patches) {
		auto& sa = saMap[name];
		sa.setParams(logger.context(L"Proc '{}': ", name), data, legacyNumber, samplesPerSec, channelLayout);
		sa.configureSnapshot(dataSnapshot[name]);
	}
}

void ProcessingOrchestrator::process(const ChannelMixer& channelMixer) {
	using clock = std::chrono::high_resolution_clock;
	static_assert(clock::is_steady);
	using namespace std::chrono_literals;

	const auto killTime = clock::now() + std::chrono::duration_cast<std::chrono::milliseconds>(1.0ms * killTimeoutMs);

	bool killed = false;
	const auto processBeginTime = clock::now();
	for (auto& [name, sa] : saMap) {
		killed = sa.process(channelMixer, killTime);
		if (killed) {
			break;
		}
	}
	const auto processEndTime = clock::now();

	for (auto& [name, sa] : saMap) {
		sa.updateSnapshot(dataSnapshot[name]);
	}

	const auto processDuration = std::chrono::duration<double, std::milli>{ processEndTime - processBeginTime }.count();

	if (killed) {
		logger.error(L"handler processing was killed on timeout after {} ms", processDuration);
		return;
	}

	if (warnTimeMs >= 0.0 && processDuration > warnTimeMs) {
		logger.debug(
			L"processing overhead {} ms over specified {} ms",
			processDuration - warnTimeMs,
			warnTimeMs
		);
	}
}

void ProcessingOrchestrator::configureSnapshot(DataSnapshot& snapshot) const {
	snapshot = dataSnapshot;
}
