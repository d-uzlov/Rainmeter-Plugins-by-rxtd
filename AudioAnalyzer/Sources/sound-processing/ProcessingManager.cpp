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
	for (auto& [channel, channelData] : channels) {
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
	channelSetRequested = pd.channels;
	legacyNumber = _legacyNumber;

	cph.setParams(pd.fcc, pd.targetRate, sampleRate);


	utils::MapUtils::intersectKeysWithPredicate(channels, [&](Channel channel) {
		return channel == Channel::eAUTO || layout.contains(channel);
	});

	std::set<Channel> channelsSet;
	// Create missing channels
	for (const auto channel : channelSetRequested) {
		const bool exists = channel == Channel::eAUTO || layout.contains(channel);
		if (exists) {
			channels[channel];
			channelsSet.insert(channel);
		}
	}
	cph.setChannels(channelsSet);


	class HandlerFinderImpl : public HandlerFinder {
		const ChannelData& channelData;

	public:
		explicit HandlerFinderImpl(const ChannelData& channelData) : channelData(channelData) {
		}

		[[nodiscard]]
		SoundHandler* getHandler(isview id) const override {
			const auto iter = channelData.find(id);
			return iter == channelData.end() ? nullptr : iter->second.get();
		}
	};


	for (auto& [channel, channelData] : channels) {
		ChannelData newData;
		HandlerFinderImpl hf{ newData };

		order = pd.handlersInfo.order;
		for (auto iter = order.begin();
		     iter != order.end();) {
			auto& handlerName = *iter;

			auto& patchInfo = *pd.handlersInfo.patchers.find(handlerName)->second;
			auto handlerPtr = patchInfo.fun(std::move(channelData[handlerName]));

			auto cl = logger.context(L"handler '{}': ", handlerName);
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				channel.technicalName(), cph.getSampleRate(),
				hf, cl
			);
			if (!success) {
				cl.error(L"invalid handler");

				if (legacyNumber < 104) {
					iter = order.erase(iter);
					continue;
				} else {
					order.clear();
					newData.clear();
					break;
				}
			}

			newData[handlerName] = std::move(handlerPtr);

			++iter;
		}

		channelData = std::move(newData);
	}
}

bool ProcessingManager::process(const ChannelMixer& mixer, clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
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
	utils::MapUtils::intersectKeyCollection(snapshot, channels);

	for (auto& [channel, channelData] : channels) {
		auto& channelSnapshot = snapshot[channel];
		utils::MapUtils::intersectKeyCollection(channelSnapshot, channelData);

		for (auto& [handlerName, handler] : channelData) {
			auto& handlerSnapshot = channelSnapshot[handlerName];
			handler->configureSnapshot(handlerSnapshot);
		}
	}
}
