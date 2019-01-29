/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Channel.h"
#include <my-windows.h>
#include <Audioclient.h>

#include "undef.h"

using namespace audio_analyzer;

ChannelLayoutKeeper audio_analyzer::layoutKeeper { };
Channel::ChannelParser Channel::channelParser { };

Channel::ChannelParser::ChannelParser() {
	addElement(L"Auto", AUTO);
	addElement(L"FrontLeft", FRONT_LEFT);
	addElement(L"FL", FRONT_LEFT);
	addElement(L"FrontRight", FRONT_RIGHT);
	addElement(L"FR", FRONT_RIGHT);
	addElement(L"Center", CENTER);
	addElement(L"C", CENTER);
	addElement(L"LowFrequency", LOW_FREQUENCY);
	addElement(L"LFE", LOW_FREQUENCY);
	addElement(L"BackLeft", BACK_LEFT);
	addElement(L"BL", BACK_LEFT);
	addElement(L"BackRight", BACK_RIGHT);
	addElement(L"BR", BACK_RIGHT);
	addElement(L"SideLeft", SIDE_LEFT);
	addElement(L"SL", SIDE_LEFT);
	addElement(L"SideRight", SIDE_RIGHT);
	addElement(L"SR", SIDE_RIGHT);
}

std::optional<Channel> Channel::ChannelParser::find(const sview str) {
	const auto iter = map.find(str % ciView());

	if (iter == map.end()) {
		return std::nullopt;
	}

	return iter->second;
}

void Channel::ChannelParser::addElement(isview name, Channel value) {
	map[name % own()] = value;
}

Channel::Channel(Value value) : value(value) { }

bool Channel::operator==(Channel a) const {
	return value == a.value;
}

bool Channel::operator!=(Channel a) const {
	return value != a.value;
}

index Channel::toInt() const {
	return value;
}

const wchar_t* Channel::technicalName() const {
	switch (value) {
	case FRONT_LEFT: return L"FRONT_LEFT";
	case FRONT_RIGHT:return L"FRONT_RIGHT";
	case CENTER: return L"CENTER";
	case LOW_FREQUENCY:return L"LOW_FREQUENCY";
	case BACK_LEFT: return L"BACK_LEFT";
	case BACK_RIGHT: return L"BACK_RIGHT";
	case SIDE_LEFT: return L"SIDE_LEFT";
	case SIDE_RIGHT: return L"SIDE_RIGHT";
	case AUTO: return L"AUTO";
	default: return L"unknown_channel";
	}
}

bool audio_analyzer::operator<(Channel left, Channel right) {
	return left.value < right.value;
}


const string& ChannelLayout::getName() const {
	return name;
}

std::optional<index> ChannelLayout::fromChannel(Channel channel) const {
	const auto iter = forward.find(channel);
	if (iter == forward.end()) {
		return std::nullopt;
	}
	return iter->second;
}

const std::unordered_set<Channel>& ChannelLayout::channelsView() const {
	return channels;
}

const ChannelLayout* ChannelLayoutKeeper::getMono() const {
	return &mono;
}

const ChannelLayout* ChannelLayoutKeeper::getStereo() const {
	return &stereo;
}

const ChannelLayout* ChannelLayoutKeeper::layoutFromChannelMask(uint32_t mask) const {
	switch (mask) {
	case KSAUDIO_SPEAKER_MONO:				return &mono;
	case KSAUDIO_SPEAKER_1POINT1:			return &_1_1;
	case KSAUDIO_SPEAKER_STEREO:			return &stereo;
	case KSAUDIO_SPEAKER_2POINT1:			return &_2_1;
	case KSAUDIO_SPEAKER_3POINT0:			return &_3_0;
	case KSAUDIO_SPEAKER_3POINT1:			return &_3_1;
	case KSAUDIO_SPEAKER_QUAD:				return &quad;
	case KSAUDIO_SPEAKER_5POINT0:			return &_5_0;
	case KSAUDIO_SPEAKER_5POINT1_SURROUND: [[fallthrough]];
	case KSAUDIO_SPEAKER_5POINT1:			return &_5_1;
	case KSAUDIO_SPEAKER_7POINT0:			return &_7_0;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND:	return &_7_1surround;
	default:
		return nullptr;
	}
}
