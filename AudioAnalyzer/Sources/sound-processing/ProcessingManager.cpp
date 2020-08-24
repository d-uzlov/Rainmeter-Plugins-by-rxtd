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

void ProcessingManager::setParams(
	utils::Rainmeter::Logger logger,
	const ParamParser::ProcessingData& pd,
	index legacyNumber,
	index sampleRate, ChannelLayout layout
) {
	std::set<Channel> channels;
	for (const auto channel : pd.channels) {
		if (channel == Channel::eAUTO || layout.contains(channel)) {
			channels.insert(channel);
		}
	}

	auto oldChannelMap = std::exchange(channelMap, { });

	resamplingData.setParams(sampleRate, pd.targetRate);
	for (auto channel : channels) {
		// todo preserve state?
		auto& newChannelStruct = channelMap[channel];
		newChannelStruct.fc = pd.fcc.getInstance(double(resamplingData.finalSampleRate));
		newChannelStruct.downsampleHelper.setFactor(resamplingData.divider);
	}

	order.clear();
	for (auto& handlerName : pd.handlersInfo.order) {
		auto& patchInfo = *pd.handlersInfo.patchers.find(handlerName)->second;

		bool handlerIsValid = true;
		for (auto channel : channels) {
			auto& channelDataNew = channelMap[channel].handlerMap;
			auto handlerPtr = patchInfo.fun(std::move(oldChannelMap[channel].handlerMap[handlerName]));

			auto cl = logger.context(L"handler '{}': ", handlerName);
			HandlerFinderImpl hf{ channelDataNew };
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				channel.technicalName(), resamplingData.finalSampleRate,
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
	for (auto& [channel, channelStruct] : channelMap) {
		auto wave = mixer.getChannelPCM(channel);

		if (resamplingData.divider <= 1) {
			waveBuffer.resize(wave.size());
			std::copy(wave.begin(), wave.end(), waveBuffer.begin());
		} else {
			const index nextBufferSize = channelStruct.downsampleHelper.pushData(wave);
			waveBuffer.resize(nextBufferSize);
			channelStruct.downsampleHelper.downsample(waveBuffer);
		}

		channelStruct.fc.applyInPlace(waveBuffer);

		for (auto& handlerName : order) {
			auto& handler = *channelStruct.handlerMap[handlerName];
			handler.process(waveBuffer, killTime);

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

void ProcessingManager::configureSnapshot(Snapshot& snapshot) {
	utils::MapUtils::intersectKeyCollection(snapshot, channelMap);

	for (auto& [channel, channelStruct] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		utils::MapUtils::intersectKeyCollection(channelSnapshot, channelStruct.handlerMap);

		for (auto& [handlerName, handler] : channelStruct.handlerMap) {
			auto& handlerSnapshot = channelSnapshot[handlerName];
			handler->configureSnapshot(handlerSnapshot);
		}
	}
}

void ProcessingManager::updateSnapshot(Snapshot& snapshot) {
	for (auto&[channel, channelStruct] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		for (auto&[handlerName, handler] : channelStruct.handlerMap) {
			// order is not important
			handler->updateSnapshot(channelSnapshot[handlerName]);
		}
	}
}
