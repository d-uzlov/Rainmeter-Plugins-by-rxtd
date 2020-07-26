/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SoundAnalyzer.h"

#include "undef.h"

using namespace audio_analyzer;

void SoundAnalyzer::setLayout(ChannelLayout _layout) {
	if (layout == _layout) {
		return;
	}

	layout = std::move(_layout);

	removeNonexistentChannelsFromMap();

	// add channels that didn't exist
	for (auto[newChannel, _] : layout.getChannelsMapView()) {
		channels[newChannel];
	}
	channels[Channel::eAUTO];

	patchHandlers();
}

void SoundAnalyzer::setSampleRate(index value) {
	if (sampleRate == value) {
		return;
	}

	sampleRate = value;
	updateSampleRate();
}

AudioChildHelper SoundAnalyzer::getAudioChildHelper() const {
	return audioChildHelper;
}

void SoundAnalyzer::setHandlerPatchers(
	std::map<Channel, std::vector<istring>> handlersOrder,
	std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>> patchers
) {

	this->patchers = std::move(patchers);
	this->orderOfHandlers = std::move(handlersOrder);

	patchHandlers();
}

void SoundAnalyzer::process(const ChannelMixer& mixer, bool isSilent) {
	if (channels.empty()) {
		return;
	}

	dataSupplier.setWaveSize(mixer.getWaveSize());
	dataSupplier.logger = logger;

	for (auto& [channel, channelData] : channels) {
		if (channelData.handlers.empty()) {
			continue;
		}

		dataSupplier.setChannelData(&channelData);

		if (isSilent) {
			for (auto& handler : channelData.handlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			dataSupplier.setWave(mixer.getChannelPCM(channel));
			for (auto& handler : channelData.handlers) {
				handler->process(dataSupplier);
			}
		}
		dataSupplier.resetBuffers();
	}
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [_, channel] : channels) {
		for (auto& handler : channel.handlers) {
			handler->reset();
		}
	}
}

void SoundAnalyzer::finishStandalone() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& handler : channelData.handlers) {
			if (handler->isStandalone()) {
				handler->finish(dataSupplier);
			}
		}
	}
}

void SoundAnalyzer::updateSampleRate() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& handler : channelData.handlers) {
			handler->setSamplesPerSec(sampleRate);
		}
	}
}

void SoundAnalyzer::removeNonexistentChannelsFromMap() {
	std::vector<Channel> toDelete;

	for (const auto& channelIter : channels) {
		Channel c = channelIter.first;

		if (c == Channel::eAUTO) {
			continue;
		}

		if (!layout.contains(c)) {
			toDelete.push_back(c);
		}
	}

	for (auto c : toDelete) {
		channels.erase(c);
	}
}

void SoundAnalyzer::patchHandlers() {
	for (auto& [channel, channelData] : channels) {
		auto orderListIter = orderOfHandlers.find(channel);
		if (orderListIter == orderOfHandlers.end()) {
			// this channel doesn't have any handlers
			// remove all handlers that it could have had earlier
			channelData = { };
			continue;
		}

		decltype(ChannelData::handlers) newHandlers;
		decltype(ChannelData::indexMap) newIndexMap;

		auto& orderList = orderListIter->second;
		index index = 0;

		for (auto& handlerName : orderList) {
			auto patcherIter = patchers.find(handlerName);
			if (patcherIter == patchers.end()) {
				continue;
			}

			auto& patcher = patcherIter->second;
			std::unique_ptr<SoundHandler> handler;

			auto handlerIndexIter = channelData.indexMap.find(handlerName);
			if (handlerIndexIter == channelData.indexMap.end()) {
				handler = std::unique_ptr<SoundHandler>(patcher(nullptr, channel));
			} else {
				std::unique_ptr<SoundHandler>& oldHandler = channelData.handlers[handlerIndexIter->second];
				SoundHandler* res = patcher(oldHandler.get(), channel);
				if (res != oldHandler.get()) {
					oldHandler.reset();
					handler = std::unique_ptr<SoundHandler>(res);
				} else {
					handler = std::move(oldHandler);
				}
			}

			newHandlers.push_back(std::move(handler));
			newIndexMap[handlerName] = index;
			index++;
		}

		channelData.handlers = std::move(newHandlers);
		channelData.indexMap = std::move(newIndexMap);
	}

	updateSampleRate();
}
