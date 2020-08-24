/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ProcessingManager.h"

#include "MapUtils.h"

using namespace audio_analyzer;

void ProcessingManager::updateSnapshot(Snapshot& snapshot) {
	for (auto& [channel, channelData] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		for (auto& [handlerName, handler] : channelData) {
			// order is not important
			handler->updateSnapshot(channelSnapshot[handlerName]);
		}
	}
}

void ProcessingManager::setParams(
	Logger logger,
	const ParamParser::ProcessingData& pd,
	index _legacyNumber,
	index sampleRate, ChannelLayout layout
) {
	legacyNumber = _legacyNumber;

	cph.setParams(pd.fcc, pd.targetRate, sampleRate);

	std::set<Channel> channels;
	for (const auto channel : pd.channels) {
		if (channel == Channel::eAUTO || layout.contains(channel)) {
			channels.insert(channel);
		}
	}

	cph.setChannels(channels);

	auto oldChannelMap = std::exchange(channelMap, { });

	order.clear();
	for (auto& handlerName : pd.handlersInfo.order) {
		auto& patchInfo = *pd.handlersInfo.patchers.find(handlerName)->second;

		bool handlerIsValid = true;
		for (auto channel : channels) {
			auto& channelDataNew = channelMap[channel];
			auto handlerPtr = patchInfo.fun(std::move(oldChannelMap[channel][handlerName]));

			auto cl = logger.context(L"handler '{}': ", handlerName);
			HandlerFinderImpl hf{ channelDataNew };
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				channel.technicalName(), cph.getSampleRate(),
				hf, cl
			);

			if (!success) {
				cl.error(L"invalid handler");

				handlerIsValid = false;
			}

			channelDataNew[handlerName] = std::move(handlerPtr);
		}

		if (handlerIsValid) {
			order.push_back(handlerName);
		} else {
			if (legacyNumber < 104) {
				continue;
			}

			order.clear();
			break;
		}
	}
}

bool ProcessingManager::process(const ChannelMixer& mixer, clock::time_point killTime) {
	for (auto& [channel, channelData] : channelMap) {
		cph.processDataFrom(channel, mixer.getChannelPCM(channel));

		const auto wave = cph.getResampled();

		for (auto& handlerName : order) {
			auto& handler = *channelData[handlerName];
			handler.process(wave, killTime);

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

void ProcessingManager::configureSnapshot(Snapshot& snapshot) {
	utils::MapUtils::intersectKeyCollection(snapshot, channelMap);

	for (auto& [channel, channelData] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		utils::MapUtils::intersectKeyCollection(channelSnapshot, channelData);

		for (auto& [handlerName, handler] : channelData) {
			auto& handlerSnapshot = channelSnapshot[handlerName];
			handler->configureSnapshot(handlerSnapshot);
		}
	}
}
