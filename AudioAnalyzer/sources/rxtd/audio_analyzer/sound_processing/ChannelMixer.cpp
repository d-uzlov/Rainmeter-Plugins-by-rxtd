// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "ChannelMixer.h"
#include "Channel.h"

#include <set>

using rxtd::audio_analyzer::ChannelMixer;

void ChannelMixer::setLayout(const ChannelLayout& _layout) {
	if (layout == _layout) {
		return;
	}

	layout = _layout;

	auto left = layout.indexOf(Channel::eFRONT_LEFT);
	auto right = layout.indexOf(Channel::eFRONT_RIGHT);
	auto center = layout.indexOf(Channel::eCENTER);

	if (left.has_value() && right.has_value()) {
		aliasOfAuto = Channel::eAUTO;
	} else if (left.has_value()) {
		aliasOfAuto = Channel::eFRONT_LEFT;
	} else if (right.has_value()) {
		aliasOfAuto = Channel::eFRONT_RIGHT;
	} else if (center.has_value()) {
		aliasOfAuto = Channel::eCENTER;
	} else {
		aliasOfAuto = layout.ordered()[0];
	}

	std::vector<Channel> toDelete;
	for (const auto& [channel, _] : channels) {
		const bool exists = channel == Channel::eAUTO || layout.contains(channel);
		if (!exists) {
			toDelete.push_back(channel);
		}
	}
	for (auto c : toDelete) {
		channels.erase(c);
	}

	// Create missing channels
	for (const auto channel : layout.ordered()) {
		channels[channel];
	}
	channels[aliasOfAuto];
}

void ChannelMixer::saveChannelsData(std_fixes::array2d_view<float> channelsData) {
	for (auto channel : layout.ordered()) {
		auto channelData = channelsData[layout.indexOf(channel).value()];
		auto& waveBuffer = channels[channel];
		auto writeBuffer = waveBuffer.allocateNext(channelData.size());
		channelData.transferToSpan(writeBuffer);
	}
}

void ChannelMixer::createAuto() {
	if (aliasOfAuto != Channel::eAUTO) {
		return;
	}

	const auto& bufferFirst = channels[Channel::eFRONT_LEFT].getAllData();
	const auto& bufferSecond = channels[Channel::eFRONT_RIGHT].getAllData();

	const index size = bufferFirst.size();
	auto writeBuffer = channels[Channel::eAUTO].allocateNext(size);

	for (index i = 0; i < size; ++i) {
		writeBuffer[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}

array_view<float> ChannelMixer::getChannelPCM(Channel channel) const {
	if (channel == Channel::eAUTO) {
		channel = aliasOfAuto;
	}

	return channels.at(channel).getAllData();
}
