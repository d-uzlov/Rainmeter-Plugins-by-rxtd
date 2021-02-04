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
	Logger logger,
	const ParamParser::ProcessingData& pd,
	Version version,
	index sampleRate, ChannelLayout layout,
	Snapshot& snapshot
) {
	std::set<Channel> channels;
	for (const auto channel : pd.channels) {
		if (channel == Channel::eAUTO || layout.contains(channel)) {
			channels.insert(channel);
		}
	}

	if (pd.targetRate == 0) {
		resamplingDivider = 1;
	} else {
		const auto ratio = static_cast<double>(sampleRate) / pd.targetRate;
		resamplingDivider = ratio > 1 ? static_cast<index>(ratio) : 1;
	}
	const index finalSampleRate = sampleRate / resamplingDivider;

	auto oldChannelMap = std::exchange(channelMap, {});

	for (auto channel : channels) {
		auto& newChannelStruct = channelMap[channel];
		newChannelStruct.filter = pd.fcc.getInstance(double(finalSampleRate));
		newChannelStruct.downsampleHelper.setFactor(resamplingDivider);
	}

	order.clear();
	for (auto& handlerName : pd.handlersInfo.order) {
		auto& patchInfo = pd.handlersInfo.patchers.find(handlerName)->second;

		bool handlerIsValid = true;
		for (auto channel : channels) {
			auto& channelDataNew = channelMap[channel].handlerMap;
			auto handlerPtr = patchInfo.fun(std::move(oldChannelMap[channel].handlerMap[handlerName]));

			auto cl = logger.context(L"handler '{}': ", handlerName);
			HandlerFinderImpl hf{ channelDataNew };
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				finalSampleRate, version,
				hf, cl,
				snapshot[channel][handlerName]
			);

			if (!success) {
				cl.error(L"invalid handler");

				handlerIsValid = false;
				break;
			}

			channelDataNew[handlerName] = std::move(handlerPtr);
		}

		if (handlerIsValid) {
			order.push_back(handlerName);
		} else {
			if (version < Version::eVERSION2) {
				continue;
			}

			order.clear();
			break;
		}
	}

	utils::MapUtils::intersectKeyCollection(snapshot, channels);
	for (auto& [channel, channelStruct] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		utils::MapUtils::intersectKeyCollection(channelSnapshot, channelStruct.handlerMap);
	}

	for (auto& [channel, channelHandlers] : channelMap) {
		for (auto& handlerName : order) {
			channelHandlers.handlerMap[handlerName]->finishConfiguration();
		}
	}
}

void ProcessingManager::process(const ChannelMixer& mixer, clock::time_point killTime, Snapshot& snapshot) {
	for (auto& [channel, channelStruct] : channelMap) {
		auto& channelSnapshot = snapshot[channel];

		SoundHandlerBase::ProcessContext context{};

		if (auto wave = mixer.getChannelPCM(channel);
			resamplingDivider <= 1) {
			context.originalWave = wave;
		} else {
			const index nextBufferSize = channelStruct.downsampleHelper.pushData(wave);
			downsampledBuffer.resize(nextBufferSize);
			channelStruct.downsampleHelper.downsample(downsampledBuffer);
			context.originalWave = downsampledBuffer;
		}
		context.originalWave.transferToVector(filteredBuffer);

		channelStruct.filter.applyInPlace(filteredBuffer);

		context.wave = filteredBuffer;
		context.killTime = killTime;

		for (auto& handlerName : order) {
			auto& handler = *channelStruct.handlerMap[handlerName];
			handler.process(context, channelSnapshot[handlerName]);
		}
	}
}
