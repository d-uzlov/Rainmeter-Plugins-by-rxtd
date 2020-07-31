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
			channels[channel];
		}
	}

	// TODO check if it is even needed?
	updateFilters();
}

void ChannelProcessingHelper::setParams(audio_utils::FilterCascadeCreator _fcc, index targetRate) {
	if (fcc == _fcc
		&& resamplingData.targetRate == targetRate) {
		return;
	}

	fcc = std::move(_fcc);
	resamplingData.targetRate = targetRate;

	recalculateResamplingData();
	updateFilters();
}

void ChannelProcessingHelper::setSourceRate(index value) {
	if (value == 0 || value == resamplingData.sourceRate) {
		return;
	}

	resamplingData.sourceRate = value;
	recalculateResamplingData();
	updateFilters();
}

void ChannelProcessingHelper::reset() const {
	for (auto& [channel, data] : channels) {
		data.wave.compact();
		data.preprocessed = false;
	}
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

	array_span<float> writeBuffer;
	if (resamplingData.divider <= 1) {
		writeBuffer = data.wave.allocateNext(wave.size());
		std::copy(wave.begin(), wave.end(), writeBuffer.begin());
	} else {
		const index nextBufferSize = data.downsampleHelper.calcBufferSizeFor(wave.size());
		writeBuffer = data.wave.allocateNext(nextBufferSize);
		(void)data.downsampleHelper.resample(wave, writeBuffer);
	}

	data.fc.applyInPlace(writeBuffer);
	data.preprocessed = true;
}

void ChannelProcessingHelper::recalculateResamplingData() {
	if (resamplingData.targetRate == 0 || resamplingData.sourceRate == 0) {
		resamplingData.divider = 1;
	} else {
		const auto ratio = static_cast<double>(resamplingData.sourceRate) / resamplingData.targetRate;
		resamplingData.divider = ratio > 1 ? static_cast<index>(ratio) : 1;
	}
	resamplingData.finalSampleRate = resamplingData.sourceRate / resamplingData.divider;
}

void ChannelProcessingHelper::updateFilters() {
	if (resamplingData.finalSampleRate == 0) {
		return;
	}

	for (auto& [c, cd] : channels) {
		cd.fc = fcc.getInstance(double(resamplingData.finalSampleRate));
		cd.downsampleHelper.setFactor(resamplingData.divider);
	}
}
