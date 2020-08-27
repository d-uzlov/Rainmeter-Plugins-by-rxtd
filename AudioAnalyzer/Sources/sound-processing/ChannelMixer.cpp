/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ChannelMixer.h"
#include "Channel.h"

#include <set>

using namespace audio_analyzer;

void ChannelMixer::setFormat(MyWaveFormat _waveFormat) {
	if (waveFormat == _waveFormat) {
		return;
	}

	waveFormat = std::move(_waveFormat);

	auto left = waveFormat.channelLayout.indexOf(Channel::eFRONT_LEFT);
	auto right = waveFormat.channelLayout.indexOf(Channel::eFRONT_RIGHT);
	auto center = waveFormat.channelLayout.indexOf(Channel::eCENTER);

	if (left.has_value() && right.has_value()) {
		aliasOfAuto = Channel::eAUTO;
	} else if (left.has_value()) {
		aliasOfAuto = Channel::eFRONT_LEFT;
	} else if (right.has_value()) {
		aliasOfAuto = Channel::eFRONT_RIGHT;
	} else if (center.has_value()) {
		aliasOfAuto = Channel::eCENTER;
	} else {
		aliasOfAuto = waveFormat.channelLayout.ordered()[0]; // todo when empty there will be a crash
	}

	std::vector<Channel> toDelete;
	for (const auto& [channel, _] : channels) {
		const bool exists = channel == Channel::eAUTO || waveFormat.channelLayout.contains(channel);
		if (!exists) {
			toDelete.push_back(channel);
		}
	}
	for (auto c : toDelete) {
		channels.erase(c);
	}

	// Create missing channels
	for (const auto channel : waveFormat.channelLayout.ordered()) {
		channels[channel];
	}
	channels[aliasOfAuto];
}

void ChannelMixer::saveChannelsData(utils::array2d_view<float> channelsData, bool withAuto) {
	for (auto channel : waveFormat.channelLayout.ordered()) {
		auto channelData = channelsData[waveFormat.channelLayout.indexOf(channel).value()];
		auto& waveBuffer = channels[channel];
		auto writeBuffer = waveBuffer.allocateNext(channelData.size());
		std::copy(channelData.begin(), channelData.end(), writeBuffer.begin());
	}

	if (withAuto && aliasOfAuto == Channel::eAUTO) {
		const auto valuesCount = channelsData[0].size();
		resampleToAuto(valuesCount);
	}
}

array_view<float> ChannelMixer::getChannelPCM(Channel channel) const {
	if (channel == Channel::eAUTO) {
		channel = aliasOfAuto;
	}

	return channels.at(channel).getAllData();
}

void ChannelMixer::resampleToAuto(index size) {
	auto writeBuffer = channels[Channel::eAUTO].allocateNext(size);
	const auto& bufferFirst = channels[Channel::eFRONT_LEFT].getLast(size);
	const auto& bufferSecond = channels[Channel::eFRONT_RIGHT].getLast(size);

	for (index i = 0; i < size; ++i) {
		writeBuffer[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}
