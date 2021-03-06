// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "ProcessingManager.h"

#include "rxtd/std_fixes/MapUtils.h"

using rxtd::audio_analyzer::ProcessingManager;
using rxtd::std_fixes::MapUtils;
using rxtd::audio_analyzer::options::HandlerInfo;

void ProcessingManager::setParams(
	Logger _logger,
	const ProcessingData& pd,
	Version version,
	index sampleRate, array_view<Channel> channelsView,
	Snapshot& snapshot
) {
	logger = std::move(_logger);

	std::set<Channel> channels;
	for (const auto channel : pd.channels) {
		if (channel == Channel::eAUTO || channelsView.contains(channel)) {
			channels.insert(channel);
		}
	}

	if (pd.targetRate == 0) {
		resamplingDivider = 1;
	} else {
		const auto ratio = static_cast<double>(sampleRate) / static_cast<double>(pd.targetRate);
		resamplingDivider = ratio > 1 ? static_cast<index>(ratio) : 1;
	}
	const index finalSampleRate = sampleRate / resamplingDivider;

	auto oldChannelMap = std::exchange(channelMap, {});

	for (auto channel : channels) {
		auto& newChannelStruct = channelMap[channel];
		auto& oldChannelStruct = oldChannelMap[channel];
		if (oldChannelStruct.filterSource != newChannelStruct.filterSource) {
			newChannelStruct.filter = std::move(oldChannelStruct.filter);
		} else {
			newChannelStruct.filter = pd.filter.creator.getInstance(static_cast<double>(finalSampleRate));
		}
		newChannelStruct.downsampleHelper.setFactor(resamplingDivider);
	}

	order.clear();
	for (auto& handlerName : pd.handlerOrder) {
		const HandlerInfo& patchInfo = pd.handlers.find(handlerName)->second;

		bool handlerIsValid = true;
		for (auto channel : channels) {
			auto& channelDataNew = channelMap[channel].handlerMap;
			auto cl = logger.context(L"{}: ", handlerName);

			auto handlerPtr = patchInfo.meta.transform(std::move(oldChannelMap[channel].handlerMap[handlerName]));
			if (handlerPtr == nullptr) {
				cl.error(L"invalid handler");

				handlerIsValid = false;
				break;
			}

			handler::HandlerBase* source = nullptr;
			if (patchInfo.meta.sourcesCount == 1) {
				const auto iter = channelDataNew.find(patchInfo.source);
				if (iter == channelDataNew.end()) {
					cl.error(L"source (handler {}) is not found", patchInfo.source);
					handlerIsValid = false;
					break;
				}
				source = iter->second.get();
			}
			const bool success = handlerPtr->patch(
				handlerName % csView() % own(),
				patchInfo.meta.params, source,
				finalSampleRate, version,
				cl,
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
			order.clear();
			break;
		}
	}

	MapUtils::intersectKeyCollection(snapshot, channels);
	for (auto& [channel, channelStruct] : channelMap) {
		auto& channelSnapshot = snapshot[channel];
		MapUtils::intersectKeyCollection(channelSnapshot, channelStruct.handlerMap);
	}
}

void ProcessingManager::process(const ChannelMixer& mixer, clock::time_point killTime, Snapshot& snapshot) {
	try {
		for (auto& [channel, channelStruct] : channelMap) {
			auto& channelSnapshot = snapshot[channel];

			handler::HandlerBase::ProcessContext context{};

			if (auto wave = mixer.getChannelPCM(channel);
				resamplingDivider <= 1) {
				context.originalWave = wave;
			} else {
				const index nextBufferSize = channelStruct.downsampleHelper.pushData(wave);
				downsampledBuffer.resize(static_cast<size_t>(nextBufferSize));
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
	} catch (handler::HandlerBase::TooManyValuesException& e) {
		logger.error(L"{}: memory usage exceeded limit. Check your settings", e.getSourceName());
		logger.error(L"processing stopped");
		channelMap.clear();
	} catch (handler::HandlerBase::InvalidOptionsException&) {
		logger.error(L"{}: unknown runtime error");
		logger.error(L"processing stopped");
		channelMap.clear();
	}
}
