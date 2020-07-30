/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ChannelProcessingHelper.h"
#include "Channel.h"

#include "undef.h"

using namespace audio_analyzer;

void ChannelProcessingHelper::setChannels(const std::set<Channel>& set) {
	std::vector<Channel> toDelete;
	for (auto& [channel, data] : channels) {
		if (set.count(channel) < 1) {
			toDelete.push_back(channel);
		}
	}
	for (auto channel : toDelete) {
		channels.erase(channel);
	}// TODO check sample rate != 0
	for (auto channel : set) {
		if (channels.count(channel) < 1) {
			const index sampleRate = resampler.getSampleRate();
			channels[channel].fc = fcc.getInstance(double(sampleRate));
		}
	}
}

void ChannelProcessingHelper::setFCC(audio_utils::FilterCascadeCreator value) {
	if (fcc == value) {
		return;
	}

	fcc = std::move(value);
	updateFC();
}

void ChannelProcessingHelper::cacheChannel() const {
	auto& data = channels[currentChannel];
	if (data.preprocessed) {
		return;
	}

	auto wave = mixer->getChannelPCM(currentChannel);
	if (wave.empty()) {
		return;
	}

	auto writeBuffer = data.wave.allocateNext(resampler.calculateFinalWaveSize(wave.size()));
	resampler.resample(wave, writeBuffer);
	data.fc.applyInPlace(writeBuffer);
	data.preprocessed = true;
}

void ChannelProcessingHelper::reset() const {
	for (auto& [channel, data] : channels) {
		data.wave.compact();
		data.preprocessed = false;
	}
}

void ChannelProcessingHelper::updateFC() {
	const index sampleRate = resampler.getSampleRate();
	if (sampleRate == 0) {
		return;
	}

	for (auto&[c, cd] : channels) {
		cd.fc = fcc.getInstance(double(sampleRate));
	}
}
