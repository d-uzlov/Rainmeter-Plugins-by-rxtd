/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SoundAnalyzer.h"

#include "undef.h"

using namespace audio_analyzer;

SoundAnalyzer::SoundAnalyzer() noexcept : audioChildHelper(channels, dataSupplier), dataSupplier(wave) {
	channelMixer.setBuffer(wave);
}

void SoundAnalyzer::setTargetRate(index value) noexcept {
	resampler.setTargetRate(value);
	updateSampleRate();
}

AudioChildHelper SoundAnalyzer::getAudioChildHelper() const {
	return audioChildHelper;
}

void SoundAnalyzer::patchHandlers(std::map<Channel, std::vector<istring>> handlersOrder,
	std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap) noexcept {

	for (const auto &channelOrder : handlersOrder) {
		auto channelIter = channels.find(channelOrder.first);
		if (channelIter == channels.end()) {
			// channel is specified in options but doesn't exist in the layout
			continue;
		}

		decltype(ChannelData::handlers) newHandlers;
		decltype(ChannelData::indexMap) newIndexMap;

		auto &channelData = channelIter->second;
		index index = 0;

		for (auto &handlerName : channelOrder.second) {
			auto iter3 = handlerPatchersMap.find(handlerName);
			if (iter3 == handlerPatchersMap.end()) {
				continue;
			}

			std::unique_ptr<SoundHandler> handler;

			auto iter2 = channelData.indexMap.find(handlerName);
			if (iter2 == channelData.indexMap.end()) {
				handler = std::unique_ptr<SoundHandler>(iter3->second(nullptr));
			} else {
				std::unique_ptr<SoundHandler> &oldHandler = channelData.handlers[iter2->second];
				SoundHandler* res = iter3->second(oldHandler.get());
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

	this->patchers = std::move(handlerPatchersMap);
	this->orderOfHandlers = std::move(handlersOrder);

	updateSampleRate();
}

void SoundAnalyzer::setWaveFormat(MyWaveFormat waveFormat) noexcept {
	if (waveFormat.format == Format::eINVALID) {
		this->waveFormat = waveFormat;
		return;
	}

	wave.setBuffersCount(waveFormat.channelsCount + 1ll);

	// remove channels that no longer exist
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

	auto channelLayoutMap = waveFormat.channelLayout.getChannelsView();
	channelLayoutMap[Channel::eAUTO] = -1;

	// add channels that didn't exist
	for (auto [newChannel, _] : channelLayoutMap) {
		// skip already existing channels
		if (channels.find(newChannel) != channels.end()) {
			continue;
		}

		// create channel data before checking existing of handlers for this channel
		// data should always exist, even if it isn't used
		auto &data = channels[newChannel];

		// keep channel empty if no handlers specified
		const auto iterOrder = orderOfHandlers.find(newChannel);
		if (iterOrder == orderOfHandlers.end()) {
			continue;
		}

		// Fill channelData with handlers
		index index = 0;
		for (const auto &handlerIndex : iterOrder->second) {
			const auto iterPatcher = patchers.find(handlerIndex);
			if (iterPatcher == patchers.end() || iterPatcher->second == nullptr) {
				continue;
			}

			SoundHandler* soundHandler = iterPatcher->second(nullptr);
			data.handlers.push_back(std::unique_ptr<SoundHandler>(soundHandler));
			data.indexMap[handlerIndex] = index;
			index++;
		}
	}

	this->waveFormat = waveFormat;
	channelMixer.setFormat(waveFormat);
	resampler.setSourceRate(waveFormat.samplesPerSec);
	updateSampleRate();
}

void SoundAnalyzer::process(const uint8_t* buffer, bool isSilent, index framesCount) noexcept {
	if (waveFormat.format == Format::eINVALID || waveFormat.channelsCount <= 0) {
		return;
	}

	if (!isSilent) {
		channelMixer.decomposeFramesIntoChannels(buffer, framesCount);
	}

	dataSupplier.setWaveSize(resampler.calculateFinalWaveSize(framesCount));

	dataSupplier.setChannelData(&channels[Channel::eAUTO]);
	dataSupplier.setChannel(Channel::eAUTO);

	auto &autoHandlers = channels[Channel::eAUTO].handlers;
	if (!autoHandlers.empty()) {
		// must be processed before other channels because wave is destroyed after resample()
		if (isSilent) {
			for (auto& handler : autoHandlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			// data.handlers always not empty
			const auto waveIndex = createChannelAuto(framesCount);
			if (waveIndex >= 0) {
				dataSupplier.setChannelIndex(waveIndex);
				resampler.resample(wave[waveIndex], framesCount);

				for (auto& handler : autoHandlers) {
					handler->process(dataSupplier);
				}
			}
		}
		dataSupplier.resetBuffers();
	}

	for (auto &channelOrderIter : orderOfHandlers) {
		const auto channel = channelOrderIter.first;
		const auto dataIter = channels.find(channel);
		if (dataIter == channels.end()) {
			continue;
		}

		auto &data = dataIter->second;

		dataSupplier.setChannelData(&data);
		dataSupplier.setChannel(channel);

		const std::optional<index> waveIndexOpt = waveFormat.channelLayout.fromChannel(channel);
		if (!waveIndexOpt.has_value()) {
			continue;
		}
		const index waveIndex = waveIndexOpt.value();

		if (waveIndex < 0 || waveIndex >= waveFormat.channelsCount) {
			continue;
		}

		dataSupplier.setChannelIndex(waveIndex);

		if (isSilent) {
			for (auto& handler : data.handlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			// data.handlers always not empty
			resampler.resample(wave[waveIndex], framesCount);

			for (auto& handler : data.handlers) {
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

void SoundAnalyzer::finish() noexcept {
	for (auto&[_, channel] : channels) {
		for (auto& handler : channel.handlers) {
			if (handler->isStandalone()) {
				handler->finish(dataSupplier);
			}
		}
	}
}

void SoundAnalyzer::updateSampleRate() noexcept {
	if (waveFormat.format == Format::eINVALID) {
		return;
	}

	const auto sampleRate = resampler.getSampleRate();
	for (auto & channel : channels) {
		for (auto &handler : channel.second.handlers) {
			handler->setSamplesPerSec(sampleRate);
		}
	}
}

index SoundAnalyzer::createChannelAuto(index framesCount) noexcept {
	auto iter = orderOfHandlers.find(Channel::eAUTO);
	if (iter == orderOfHandlers.end()) {
		return -1;
	}
	auto autoChannelOrderList = iter->second;
	if (autoChannelOrderList.empty()) {
		return -1;
	}

	const index channelIndex = wave.getBuffersCount() - 1;

	channelMixer.createChannelAuto(framesCount, channelIndex);

	return channelIndex;
}
