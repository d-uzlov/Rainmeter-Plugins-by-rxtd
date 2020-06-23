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

SoundAnalyzer::SoundAnalyzer() noexcept : audioChildHelper(channels, dataSupplier), dataSupplier(waveBuffer) {
	channelMixer.setBuffer(waveBuffer);
}

void SoundAnalyzer::setTargetRate(index value) noexcept {
	resampler.setTargetRate(value);
	updateSampleRate();
}

AudioChildHelper SoundAnalyzer::getAudioChildHelper() const {
	return audioChildHelper;
}

void SoundAnalyzer::setHandlerPatchers(std::map<Channel, std::vector<istring>> handlersOrder,
	std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> patchers) {

	this->patchers = std::move(patchers);
	this->orderOfHandlers = std::move(handlersOrder);

	patchHandlers();
}

void SoundAnalyzer::setWaveFormat(MyWaveFormat waveFormat) {
	if (waveFormat.format == utils::WaveDataFormat::eINVALID) {
		this->waveFormat = waveFormat;
		return;
	}

	removeNonexistentChannelsFromMap();

	auto channelLayoutMap = waveFormat.channelLayout.getChannelsMapView();
	channelLayoutMap[Channel::eAUTO] = -1;

	for (auto [newChannel, _] : channelLayoutMap) {
		channels[newChannel]; // add channels that didn't exist
	}

	this->waveFormat = waveFormat;
	channelMixer.setFormat(waveFormat);
	resampler.setSourceRate(waveFormat.samplesPerSec);
	waveBuffer.setBuffersCount(waveFormat.channelsCount + 1);

	patchHandlers();
}

void SoundAnalyzer::process(const uint8_t* buffer, bool isSilent, index framesCount) noexcept {
	if (waveFormat.format == utils::WaveDataFormat::eINVALID || waveFormat.channelsCount <= 0) {
		return;
	}

	if (!isSilent) {
		channelMixer.decomposeFramesIntoChannels(buffer, framesCount);

		if (!channels[Channel::eAUTO].handlers.empty()) {
			channelMixer.createChannelAuto(framesCount, waveBuffer.getBuffersCount() - 1);
		}
	}

	dataSupplier.setWaveSize(resampler.calculateFinalWaveSize(framesCount));

	for (auto &[channel, channelData] : channels) {
		if (channelData.handlers.empty()) {
			continue;
		}

		dataSupplier.setChannelData(&channelData);
		dataSupplier.setChannel(channel);

		auto channelIndexOpt = waveFormat.channelLayout.indexOf(channel);
		index waveIndex;
		if (channelIndexOpt.has_value()) {
			waveIndex = channelIndexOpt.value();
		} else {
			waveIndex = waveBuffer.getBuffersCount() - 1;
		}

		dataSupplier.setChannelIndex(waveIndex);

		if (isSilent) {
			for (auto& handler : channelData.handlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			resampler.resample(waveBuffer[waveIndex], framesCount);

			for (auto& handler : channelData.handlers) {
				handler->process(dataSupplier);
			}
		}
		dataSupplier.resetBuffers();
	}

	dataSupplier.setChannelData(nullptr);
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [_, channel] : channels) {
		for (auto& handler : channel.handlers) {
			handler->reset();
		}
	}
}

void SoundAnalyzer::finishStandalone() noexcept {
	for (auto &[channel, channelData] : channels) {
		for (auto& handler : channelData.handlers) {
			if (handler->isStandalone()) {
				handler->finish(dataSupplier);
			}
		}
	}
}

void SoundAnalyzer::updateSampleRate() noexcept {
	if (waveFormat.format == utils::WaveDataFormat::eINVALID) {
		return;
	}

	const auto sampleRate = resampler.getSampleRate();

	for (auto & [channel, channelData] : channels) {
		for (auto &handler : channelData.handlers) {
			handler->setSamplesPerSec(sampleRate);
		}
	}
}

void SoundAnalyzer::removeNonexistentChannelsFromMap() {
	std::vector<Channel> toDelete;

	for (const auto &channelIter : channels) {
		Channel c = channelIter.first;

		if (c == Channel::eAUTO) {
			continue;
		}

		if (!waveFormat.channelLayout.contains(c)) {
			toDelete.push_back(c);
		}
	}

	for (auto c : toDelete) {
		channels.erase(c);
	}
}

void SoundAnalyzer::patchHandlers() {
	for (auto &[channel, channelData] : channels) {
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

		for (auto &handlerName : orderList) {
			auto patcherIter = patchers.find(handlerName);
			if (patcherIter == patchers.end()) {
				continue;
			}

			auto& patcher = patcherIter->second;
			std::unique_ptr<SoundHandler> handler;

			auto handlerIndexIter = channelData.indexMap.find(handlerName);
			if (handlerIndexIter == channelData.indexMap.end()) {
				handler = std::unique_ptr<SoundHandler>(patcher(nullptr));
			} else {
				std::unique_ptr<SoundHandler> &oldHandler = channelData.handlers[handlerIndexIter->second];
				SoundHandler* res = patcher(oldHandler.get());
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
