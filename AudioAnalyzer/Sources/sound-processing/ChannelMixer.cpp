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

void ChannelMixer::setLayout(const ChannelLayout& _layout) {
	if (layout == _layout) {
		return;
	}

	layout = _layout;

	auto afrontLeft = layout.indexOf(Channel::eFRONT_LEFT);
	auto afrontRight = layout.indexOf(Channel::eFRONT_RIGHT);
	auto acenter = layout.indexOf(Channel::eCENTER);
	auto acenterBack = layout.indexOf(Channel::eCENTER_BACK);
	auto alfe = layout.indexOf(Channel::eLOW_FREQUENCY);
	auto abackLeft = layout.indexOf(Channel::eBACK_LEFT);
	auto abackRight = layout.indexOf(Channel::eBACK_RIGHT);
	auto asideLeft = layout.indexOf(Channel::eSIDE_LEFT);
	auto asideRight = layout.indexOf(Channel::eSIDE_RIGHT);

	if (afrontLeft.has_value() && afrontRight.has_value()) {
		aliasOfAuto = Channel::eAUTO;
	} else if (afrontLeft.has_value()) {
		aliasOfAuto = Channel::eFRONT_LEFT;
	} else if (afrontRight.has_value()) {
		aliasOfAuto = Channel::eFRONT_RIGHT;
	} else if (acenter.has_value()) {
		aliasOfAuto = Channel::eCENTER;
	} else {
		aliasOfAuto = layout.ordered()[0];
	}

	if (afrontLeft.has_value()) { frontLeft = true; }
	if (afrontRight.has_value()) { frontRight = true; }
	if (acenter.has_value()) { center = true; }
	if (acenterBack.has_value()) { centerBack = true; }
	if (alfe.has_value()) { lfe = true; }
	if (abackLeft.has_value()) { backLeft = true; }
	if (abackRight.has_value()) { backRight = true; }
	if (asideLeft.has_value()) { sideLeft = true; }
	if (asideRight.has_value()) { sideRight = true; }

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

void ChannelMixer::saveChannelsData(utils::array2d_view<float> channelsData) {
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
	const auto& bufferThird = channels[Channel::eCENTER].getAllData();
	const auto& bufferFourth = channels[Channel::eCENTER_BACK].getAllData();
	const auto& bufferFifth = channels[Channel::eLOW_FREQUENCY].getAllData();
	const auto& bufferSixth = channels[Channel::eBACK_LEFT].getAllData();
	const auto& bufferSeventh = channels[Channel::eBACK_RIGHT].getAllData();
	const auto& bufferEighth = channels[Channel::eSIDE_LEFT].getAllData();
	const auto& bufferNinth = channels[Channel::eSIDE_RIGHT].getAllData();

	const index size = bufferFirst.size();
	auto writeBuffer = channels[Channel::eAUTO].allocateNext(size);

	for (index i = 0; i < size; ++i) {
		if (frontLeft) { writeBuffer[i] += bufferFirst[i]; }
		if (frontRight) { writeBuffer[i] += bufferSecond[i]; }
		if (center) { writeBuffer[i] += bufferThird[i]; }
		if (centerBack) { writeBuffer[i] += bufferFourth[i]; }
		if (lfe) { writeBuffer[i] += bufferFifth[i]; }
		if (backLeft) { writeBuffer[i] += bufferSixth[i]; }
		if (backRight) { writeBuffer[i] += bufferSeventh[i]; }
		if (sideLeft) { writeBuffer[i] += bufferEighth[i]; }
		if (sideRight) { writeBuffer[i] += bufferNinth[i]; }
	}
}

array_view<float> ChannelMixer::getChannelPCM(Channel channel) const {
	if (channel == Channel::eAUTO) {
		channel = aliasOfAuto;
	}

	return channels.at(channel).getAllData();
}
