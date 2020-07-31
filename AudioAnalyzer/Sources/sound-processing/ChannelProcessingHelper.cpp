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

	updateFC();
}

void ChannelProcessingHelper::setParams(audio_utils::FilterCascadeCreator _fcc, index targetRate) {
	if (fcc == _fcc && resamplingData.targetRate == targetRate) {
		return;
	}

	fcc = std::move(_fcc);
	resamplingData.targetRate = targetRate;

	recalculateResamplingData();
	updateFC();
}

void ChannelProcessingHelper::setSourceRate(index value) {
	if (value == 0 || value == resamplingData.sourceRate) {
		return;
	}

	resamplingData.sourceRate = value;
	recalculateResamplingData();
	updateFC();
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

	const index nextBufferSize = (data.decimationCounter + wave.size()) / resamplingData.divider;
	auto writeBuffer = data.wave.allocateNext(nextBufferSize);

	if (resamplingData.divider <= 1) {
		std::copy(wave.begin(), wave.end(), writeBuffer.begin());
	} else {
		wave.remove_prefix(data.decimationCounter);
		const index divider = resamplingData.divider;

		const index newCount = wave.size() / divider;
		for (index i = 0; i < newCount; ++i) {
			writeBuffer[i] = wave[i * divider + divider - 1];
		}

		data.decimationCounter = wave.size() - newCount * divider;
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

void ChannelProcessingHelper::updateFC() {
	if (resamplingData.finalSampleRate == 0) {
		return;
	}

	for (auto& [c, cd] : channels) {
		cd.fc = fcc.getInstance(double(resamplingData.finalSampleRate));
	}
}
