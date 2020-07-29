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

	patch();
}

void SoundAnalyzer::setSourceRate(index value) {
	if (sampleRate == value) {
		return;
	}

	sampleRate = value;
	cph.setSourceRate(value);
	updateHandlerSampleRate();
}

const AudioChildHelper& SoundAnalyzer::getAudioChildHelper() const {
	return audioChildHelper;
}

void SoundAnalyzer::setHandlers(
	std::set<Channel> channelSetRequested,
	std::vector<ParamParser::HandlerInfo> handlerPatchers
) {

	this->channelSetRequested = std::move(channelSetRequested);
	this->handlerPatchers = std::move(handlerPatchers);

	patch();
}

void SoundAnalyzer::process() {
	cph.reset();
	dataSupplier.logger = logger;

	const index bufferSize = index(granularity * cph.getResampler().getSampleRate());
	cph.setGrabBufferSize(bufferSize);

	for (auto& [channel, channelData] : channels) {
		dataSupplier.setChannelData(&channelData);

		cph.setCurrentChannel(channel);
		while (true) {
			auto wave = cph.grabNext();
			if (wave.empty()) {
				break;
			}

			dataSupplier.setWave(wave);

			for (auto& [name, handler] : channelData) {
				handler->process(dataSupplier);
				dataSupplier.resetBuffers();
			}
		}
	}

	cph.reset();
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			handler->reset();
		}
	}
}

void SoundAnalyzer::finishStandalone() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			if (handler->isStandalone()) {
				handler->finish(dataSupplier);
			}
		}
	}
}

void SoundAnalyzer::updateHandlerSampleRate() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			handler->setSamplesPerSec(cph.getResampler().getSampleRate());
		}
	}
}

void SoundAnalyzer::patchChannels() {
	// Delete not needed channels
	std::vector<Channel> toDelete;
	for (const auto& [channel, _] : channels) {
		const bool exists = channel == Channel::eAUTO || layout.contains(channel);
		const bool isRequested = channelSetRequested.count(channel) >= 1;

		if (!exists || !isRequested) {
			toDelete.push_back(channel);
		}
	}
	for (auto c : toDelete) {
		channels.erase(c);
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

		for (auto& [handlerName, patcher] : handlerPatchers) {
			auto& handlerPtr = channelData[handlerName];

			SoundHandler* res = patcher(handlerPtr.get(), channel);
			if (res != handlerPtr.get()) {
				handlerPtr = std::unique_ptr<SoundHandler>(res);
			}

			newData[handlerName] = std::move(handlerPtr);
		}

		channelData = std::move(newData);
	}

	updateHandlerSampleRate();
}
