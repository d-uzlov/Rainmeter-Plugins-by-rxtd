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
	}
	for (auto channel : set) {
		if (channels.count(channel) < 1) {
			const index sampleRate = resampler.getSampleRate();
			channels[channel].fc = fcc.getInstance(sampleRate);
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

array_view<float> ChannelProcessingHelper::getChannelPCM(Channel channel) const {
	if (mixer == nullptr) {
		return { };
	}

	if (channel == Channel::eAUTO) {
		channel = mixer->getAutoAlias();
	}

	auto wave = mixer->getChannelPCM(channel);
	if (wave.empty()) {
		return { };
	}

	auto& data = channels[channel];
	if (!data.preprocessed) {
		data.wave.resize(resampler.calculateFinalWaveSize(wave.size()));
		resampler.resample(wave, data.wave);
		data.fc.applyInPlace(data.wave);
		data.preprocessed = true;
	}

	return data.wave;
}

void ChannelProcessingHelper::reset() const {
	for (auto& [channel, data] : channels) {
		data.preprocessed = false;
	}
}

void ChannelProcessingHelper::updateFC() {
	const index sampleRate = resampler.getSampleRate();
	if (sampleRate == 0) {
		return;
	}

	for (auto&[c, cd] : channels) {
		cd.fc = fcc.getInstance(sampleRate);
	}
}
