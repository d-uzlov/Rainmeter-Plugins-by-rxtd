/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SoundAnalyzer.h"

using namespace audio_analyzer;

void SoundAnalyzer::updateFormat(index sampleRate, ChannelLayout layout) {
	cph.updateSourceRate(sampleRate);
	patchHandlers(std::move(layout));
}

void SoundAnalyzer::setParams(
	const ParamParser::ProcessingData& pd,
	index _legacyNumber,
	index sampleRate, ChannelLayout layout
) {
	channelSetRequested = pd.channels;
	patchersInfo = pd.handlersInfo;
	legacyNumber = _legacyNumber;

	cph.setParams(pd.fcc, pd.targetRate, sampleRate);
	patchHandlers(std::move(layout));
}

bool SoundAnalyzer::process(const ChannelMixer& mixer, clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
		if (channel == Channel::eAUTO) {
			const auto alias = mixer.getAutoAlias();
			cph.processDataFrom(alias, mixer.getChannelPCM(alias));
		} else {
			cph.processDataFrom(channel, mixer.getChannelPCM(channel));
		}

		auto wave = cph.getResampled();
		if (wave.empty()) {
			// todo remove
			continue;
		}

		for (auto& handlerName : patchersInfo.order) {
			auto& handler = *channelData[handlerName];
			handler.process(wave, killTime);

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

bool SoundAnalyzer::finishStandalone(clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
		for (auto& handlerName : patchersInfo.order) {
			auto& handler = *channelData[handlerName];

			handler.finishStandalone();

			if (clock::now() > killTime) {
				return true;
			}
		}
	}

	return false;
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			// order is not important
			handler->reset();
		}
	}
}

void SoundAnalyzer::patchHandlers(ChannelLayout layout) {
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

	for (auto& [channel, channelData] : channels) {
		ChannelData newData;
		HandlerFinderImpl hf{ newData };

		auto& order = patchersInfo.order;
		for (auto iter = order.begin();
		     iter != order.end();) {
			auto& handlerName = *iter;

			auto& patchInfo = *patchersInfo.patchers.find(handlerName)->second;
			auto handlerPtr = patchInfo.fun(std::move(channelData[handlerName]));

			auto cl = logger.context(L"Handler {}: ", handlerName);
			const bool success = handlerPtr->patch(patchInfo.params, patchInfo.sources, channel.technicalName(), cph.getSampleRate(), hf, cl);
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
