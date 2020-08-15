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
	for (auto iter = channels.begin(); iter != channels.end();) {
		iter = set.count(iter->first) < 1 ? channels.erase(iter) : ++iter;
	}

	for (auto channel : set) {
		if (channels.count(channel) < 1) {
			channels[channel];
		}
	}

	// TODO check if it is even needed?
	updateFilters();
}

void ChannelProcessingHelper::setParams(
	audio_utils::FilterCascadeCreator _fcc,
	index targetRate, index sourceSampleRate
) {
	if (fcc == _fcc
		&& resamplingData.targetRate == targetRate
		&& resamplingData.sourceRate == sourceSampleRate) {
		return;
	}

	fcc = std::move(_fcc);
	resamplingData.targetRate = targetRate;
	resamplingData.sourceRate = sourceSampleRate;

	recalculateResamplingData();
	updateFilters();
}

void ChannelProcessingHelper::updateSourceRate(index value) {
	if (value == 0 || value == resamplingData.sourceRate) {
		return;
	}

	resamplingData.sourceRate = value;
	recalculateResamplingData();
	updateFilters();
}

void ChannelProcessingHelper::processDataFrom(Channel channel, array_view<float> wave) {
	if (wave.empty()) {
		buffer.clear();
		return;
	}

	auto& data = channels[channel];

	if (resamplingData.divider <= 1) {
		buffer.resize(wave.size());
		std::copy(wave.begin(), wave.end(), buffer.begin());
	} else {
		const index nextBufferSize = data.downsampleHelper.pushData(wave);
		buffer.resize(nextBufferSize);
		data.downsampleHelper.downsample(buffer);
	}

	data.fc.applyInPlace(buffer);
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
