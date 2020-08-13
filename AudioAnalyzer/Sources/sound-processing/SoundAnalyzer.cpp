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
	granularity = pd.granularity;
	legacyNumber = _legacyNumber;

	cph.setParams(pd.fcc, pd.targetRate, sampleRate);
	patchHandlers(std::move(layout));
}

bool SoundAnalyzer::process(const ChannelMixer& mixer, clock::time_point killTime) {
	const index grabSize = index(granularity * cph.getSampleRate());

	for (auto& [channel, channelData] : channels) {
		if (channel == Channel::eAUTO) {
			const auto alias = mixer.getAutoAlias();
			cph.processDataFrom(alias, mixer.getChannelPCM(alias));
		} else {
			cph.processDataFrom(channel, mixer.getChannelPCM(channel));
		}

		while (true) {
			auto wave = cph.grabNext(grabSize);
			if (wave.empty()) {
				wave = cph.grabRest();
			}
			if (wave.empty()) {
				break;
			}

			for (auto& handlerName : patchersInfo.order) {
				auto& handler = *channelData[handlerName];
				handler.process(wave);

				if (clock::now() > killTime) {
					return true;
				}
			}
		}
	}

	return false;
}

bool SoundAnalyzer::finishStandalone(clock::time_point killTime) {
	for (auto& [channel, channelData] : channels) {
		for (auto& handlerName : patchersInfo.order) {
			auto& handler = *channelData[handlerName];

			if (!handler.vIsStandalone()) {
				continue;
			}

			const bool success = handler.finish();

			if (!success) {
				logger.error(L"handler '{}' was unexpectedly invalidated, stopping processing", handlerName);
				channels.clear();
				patchersInfo.order.clear();
				return false;
			}

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

			auto& patcher = *patchersInfo.patchers.find(handlerName)->second;
			auto& handlerInfo = channelData[handlerName];

			auto cl = logger.context(L"Handler {}: ", handlerName);
			const auto ptr = SoundHandler::patch(
				handlerInfo.get(), patcher,
				channel, cph.getSampleRate(),
				hf, cl
			);

			if (ptr == nullptr) {
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

			if (ptr != handlerInfo.get()) {
				handlerInfo = std::unique_ptr<SoundHandler>(ptr);
			}

			newData[handlerName] = std::move(handlerInfo);

			++iter;
		}

		channelData = std::move(newData);
	}
}
