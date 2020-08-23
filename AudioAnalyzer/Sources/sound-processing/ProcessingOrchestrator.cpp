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

void ProcessingOrchestrator::setFormat(index samplesPerSec, ChannelLayout channelLayout) {
	for (auto& [name, sa] : saMap) {
		sa.updateFormat(samplesPerSec, channelLayout);
	}
	currentFormat.samplesPerSec = samplesPerSec;
	currentFormat.channelLayout = std::move(channelLayout);
}

void ProcessingOrchestrator::patch(const ParamParser::ProcessingsInfoMap& patches, index legacyNumber, DataSnapshot& snapshot) {
	utils::MapUtils::intersectKeyCollection(saMap, patches);
	utils::MapUtils::intersectKeyCollection(dataSnapshot, patches);
	utils::MapUtils::intersectKeyCollection(snapshot, patches);

	for (const auto& [name, data] : patches) {
		auto& sa = saMap[name];
		sa.setLogger(logger);
		sa.setParams(data, legacyNumber, currentFormat.samplesPerSec, currentFormat.channelLayout);
		sa.configureSnapshot(dataSnapshot[name]);
		sa.configureSnapshot(snapshot[name]);
	}
}

void ProcessingOrchestrator::process(const ChannelMixer& channelMixer) {
	using clock = std::chrono::high_resolution_clock;
	static_assert(clock::is_steady);

	const auto processBeginTime = clock::now();

	const std::chrono::duration<float, std::milli> processMaxDuration{ killTimeoutMs };
	const auto killTime = clock::now() + std::chrono::duration_cast<std::chrono::milliseconds>(processMaxDuration);

	bool killed = false;
	for (auto& [name, sa] : saMap) {
		killed = sa.process(channelMixer, killTime);
		if (killed) {
			break;
		}
	}

	const auto processEndTime = clock::now();

	const auto processDuration = std::chrono::duration<double, std::milli>{ processEndTime - processBeginTime }.count();

	if (killed) {
		logger.error(L"handler processing was killed on timeout after {} m, on stage 1", processDuration);
		return;
	}

	for (auto& [name, sa] : saMap) {
		sa.updateSnapshot(dataSnapshot[name]);
	}

	if (computeTimeoutMs >= 0 && processDuration > computeTimeoutMs) {
		logger.debug(
			L"processing overhead {} ms over specified {} ms",
			processDuration - computeTimeoutMs,
			computeTimeoutMs
		);
	}
}

bool ProcessingOrchestrator::getProp(
	isview proc, isview id, Channel channel,
	isview propName, utils::BufferPrinter& printer
) {
	auto procIter = saMap.find(proc);
	if (procIter == saMap.end()) {
		return { };

	}

	const auto handler = procIter->second.getAudioChildHelper().findHandler(channel, id);
	if (handler == nullptr) {
		return false;
	}

	return handler->vGetProp(propName, printer);
}
