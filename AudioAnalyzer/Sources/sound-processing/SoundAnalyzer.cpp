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

void SoundAnalyzer::setFormat(index sampleRate, ChannelLayout _layout) {
	if (sourceSampleRate == sampleRate && layout == _layout) {
		return;
	}

	sourceSampleRate = sampleRate;
	layout = std::move(_layout);

	cph.setSourceRate(sampleRate);
	patchCH();
}

void SoundAnalyzer::setParams(
	std::set<Channel> channelSetRequested,
	ParamParser::HandlerPatchersInfo _patchersInfo,
	double _granularity,
	index _legacyNumber
) {
	this->channelSetRequested = std::move(channelSetRequested);
	patchersInfo = std::move(_patchersInfo);
	granularity = _granularity;
	legacyNumber = _legacyNumber;

	patchCH();
}

bool SoundAnalyzer::process(const ChannelMixer& mixer, clock::time_point killTime) {
	cph.reset();
	cph.processDataFrom(mixer);

	cph.setGrabBufferSize(index(granularity * cph.getSampleRate()));

	for (auto& [channel, channelData] : channels) {
		cph.setCurrentChannel(channel);
		while (true) {
			auto wave = cph.grabNext();
			if (wave.empty()) {
				wave = cph.grabRest();
			}
			if (wave.empty()) {
				break;
			}

			auto& order = patchersInfo.order;
			for (auto iter = order.begin();
			     iter != order.end();) {
				auto& handlerName = *iter;

				auto& handler = *channelData[handlerName];
				handler.process(wave);

				if (!handler.isValid()) {
					logger.error(L"handler '{}' was invalidated", handlerName);
					iter = order.erase(iter);
				} else {
					++iter;
				}

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
		auto& order = patchersInfo.order;
		for (auto iter = order.begin();
		     iter != order.end();) {
			auto& handlerName = *iter;

			auto& handler = *channelData[handlerName];

			if (!handler.vIsStandalone()) {
				++iter;
				continue;
			}

			handler.finish();

			if (!handler.isValid()) {
				logger.error(L"handler '{}' was invalidated", handlerName);
				iter = order.erase(iter);
			} else {
				++iter;
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

void SoundAnalyzer::patchChannels() {
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
}

void SoundAnalyzer::patchHandlers() {
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
			SoundHandler* ptr = SoundHandler::patch(
				handlerInfo.get(), patcher,
				channel, cph.getSampleRate(),
				hf, cl
			);
			if (ptr != handlerInfo.get()) {
				handlerInfo = std::unique_ptr<SoundHandler>(ptr);
			}

			if (!ptr->isValid()) {
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

			newData[handlerName] = std::move(handlerInfo);

			++iter;
		}

		channelData = std::move(newData);
	}
}
