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

AudioChildHelper SoundAnalyzer::getAudioChildHelper() const {
	return AudioChildHelper{ channels };
}

void SoundAnalyzer::setHandlers(
	std::set<Channel> channelSetRequested,
	ParamParser::HandlerPatcherInfo handlerPatchers
) {
	this->channelSetRequested = std::move(channelSetRequested);
	this->handlerPatchers = std::move(handlerPatchers);

	patch();
}

bool SoundAnalyzer::process(const ChannelMixer& mixer, clock::time_point killTime) {
	cph.reset();
	cph.setChannelMixer(mixer);
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

			for (auto& name : handlerPatchers.order) {
				auto& handler = channelData[name];
				if (!handler.wasValid) {
					continue;
				}

				handler.ptr->process(dataSupplier);

				handler.wasValid = handler.ptr->isValid();
				if (!handler.wasValid) {
					logger.error(L"handler '{}' was invalidated", name);
				}

				if (clock::now() > killTime) {
					return true;
				}
			}
		}
	}

	return false;
}

bool SoundAnalyzer::finishStandalone(clock::time_point killTime) noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& name : handlerPatchers.order) {
			auto& handler = channelData[name];
			if (handler.wasValid && handler.ptr->isStandalone()) {
				handler.ptr->finish();
				handler.wasValid = handler.ptr->isValid();

				if (!handler.wasValid) {
					logger.error(L"handler '{}' was invalidated", name);
				}

				if (clock::now() > killTime) {
					return true;
				}
			}
		}
	}

	return false;
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			// order is not important
			handler.ptr->reset();
		}
	}
}

void SoundAnalyzer::updateHandlerSampleRate() noexcept {
	for (auto& [channel, channelData] : channels) {
		for (auto& [name, handler] : channelData) {
			// order is not important
			handler.ptr->setSamplesPerSec(cph.getResampler().getSampleRate());
			handler.wasValid = handler.ptr->isValid();
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

		for (auto& [handlerName, info] : handlerPatchers.map) {
			auto& handlerInfo = channelData[handlerName];
			auto& patcher = info.patcher;

			SoundHandler* res = patcher(handlerInfo.ptr.get(), channel);
			if (res != handlerInfo.ptr.get()) {
				handlerInfo.ptr = std::unique_ptr<SoundHandler>(res);
			}

			handlerInfo.wasValid = true;
			newData[handlerName] = std::move(handlerInfo);
		}

		channelData = std::move(newData);
	}

	updateHandlerSampleRate();
}
