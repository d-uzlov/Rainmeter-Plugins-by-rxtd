/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ProcessingManager.h"

using namespace audio_analyzer;

void ProcessingManager::updateFormat(index sampleRate, ChannelLayout layout) {
	cph.updateSourceRate(sampleRate);
	patchHandlers(std::move(layout));
}

void ProcessingManager::setParams(
	const ParamParser::ProcessingData& pd,
	index _legacyNumber,
	index sampleRate, ChannelLayout layout
) {
	channelSetRequested = pd.channels;
	patchersInfo = pd.handlersInfo;
	legacyNumber = _legacyNumber;

	cph.setParams(pd.fcc, pd.targetRate, sampleRate);
	if (sampleRate != 0) {
		patchHandlers(std::move(layout));
	} else {
		channels.clear();
	}
}

bool ProcessingManager::process(const ChannelMixer& mixer, clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
		cph.processDataFrom(channel, mixer.getChannelPCM(channel));

		const auto wave = cph.getResampled();

		for (auto& handlerName : realOrder) {
			auto& handler = *channelData[handlerName];
			handler.process(wave, killTime);

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

bool ProcessingManager::finishStandalone(clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
		for (auto& handlerName : realOrder) {
			auto& handler = *channelData[handlerName];

			handler.finishStandalone();

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

void ProcessingManager::resetValues() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			// order is not important
			handler->reset();
		}
	}
}

void ProcessingManager::patchHandlers(ChannelLayout layout) {
	for (auto iter = channels.begin(); iter != channels.end();) {
		const auto channel = iter->first;
		iter = channel == Channel::eAUTO || layout.contains(channel) ? ++iter : channels.erase(iter);
	}

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

		realOrder = patchersInfo.order;
		for (auto iter = realOrder.begin();
		     iter != realOrder.end();) {
			auto& handlerName = *iter;

			auto& patchInfo = *patchersInfo.patchers.find(handlerName)->second;
			auto handlerPtr = patchInfo.fun(std::move(channelData[handlerName]));

			auto cl = logger.context(L"Handler {}: ", handlerName);
			const bool success = handlerPtr->patch(
				patchInfo.params, patchInfo.sources,
				channel.technicalName(), cph.getSampleRate(),
				hf, cl
			);
			if (!success) {
				cl.error(L"invalid handler");

				if (legacyNumber < 104) {
					iter = realOrder.erase(iter);
					continue;
				} else {
					realOrder.clear();
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