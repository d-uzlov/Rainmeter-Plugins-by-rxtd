/*
 * Copyright (C) 2019-2020 rxtd
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

Channel::ChannelParser Channel::channelParser { };


ChannelLayout ChannelLayout::create(sview name, const std::vector<Channel::Value>& channels) {
	ChannelLayout result;

	result.name = name;

	for (index i = 0; i < static_cast<index>(channels.size()); i++) {
		result.channelMap[channels[i]] = i;
		result.channelOrder.emplace_back(channels[i]);
	}

	return result;
}

namespace {
	

	ChannelLayout mono = ChannelLayout::create(L"1.0 mono", {
		Channel::eCENTER
		});
	ChannelLayout _1_1 = ChannelLayout::create(L"1.1", {
		Channel::eCENTER, Channel::eLOW_FREQUENCY
		});
	ChannelLayout stereo = ChannelLayout::create(L"2.0 stereo", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT
		});
	ChannelLayout _2_1 = ChannelLayout::create(L"2.1", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eLOW_FREQUENCY
		});
	ChannelLayout _3_0 = ChannelLayout::create(L"3.0", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER
		});
	ChannelLayout _3_1 = ChannelLayout::create(L"3.1", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER, Channel::eLOW_FREQUENCY
		});
	ChannelLayout quad = ChannelLayout::create(L"4.0 quad", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eBACK_LEFT, Channel::eBACK_RIGHT
		});
	ChannelLayout _5_0 = ChannelLayout::create(L"5.0", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER,
		Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		});
	ChannelLayout _5_1 = ChannelLayout::create(L"5.1", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER, Channel::eLOW_FREQUENCY,
		Channel::eBACK_LEFT, Channel::eBACK_RIGHT
		});
	ChannelLayout _5_1_surround = ChannelLayout::create(L"5.1 surround", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER, Channel::eLOW_FREQUENCY,
		Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		});
	ChannelLayout _7_0 = ChannelLayout::create(L"7.0", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER,
		Channel::eBACK_LEFT, Channel::eBACK_RIGHT,
		Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		});
	ChannelLayout _7_1surround = ChannelLayout::create(L"7.1 surround", {
		Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
		Channel::eCENTER, Channel::eLOW_FREQUENCY,
		Channel::eBACK_LEFT, Channel::eBACK_RIGHT,
		Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		});
}

Channel::ChannelParser::ChannelParser() {
	addElement(L"Auto", eAUTO);
	addElement(L"Left", eFRONT_LEFT);
	addElement(L"FrontLeft", eFRONT_LEFT);
	addElement(L"FL", eFRONT_LEFT);
	addElement(L"Right", eFRONT_RIGHT);
	addElement(L"FrontRight", eFRONT_RIGHT);
	addElement(L"FR", eFRONT_RIGHT);
	addElement(L"Center", eCENTER);
	addElement(L"C", eCENTER);
	addElement(L"LowFrequency", eLOW_FREQUENCY);
	addElement(L"LFE", eLOW_FREQUENCY);
	addElement(L"BackLeft", eBACK_LEFT);
	addElement(L"BL", eBACK_LEFT);
	addElement(L"BackRight", eBACK_RIGHT);
	addElement(L"BR", eBACK_RIGHT);
	addElement(L"SideLeft", eSIDE_LEFT);
	addElement(L"SL", eSIDE_LEFT);
	addElement(L"SideRight", eSIDE_RIGHT);
	addElement(L"SR", eSIDE_RIGHT);
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

sview Channel::technicalName() const {
	switch (value) {
	case eFRONT_LEFT: return L"FRONT_LEFT";
	case eFRONT_RIGHT:return L"FRONT_RIGHT";
	case eCENTER: return L"CENTER";
	case eLOW_FREQUENCY:return L"LOW_FREQUENCY";
	case eBACK_LEFT: return L"BACK_LEFT";
	case eBACK_RIGHT: return L"BACK_RIGHT";
	case eSIDE_LEFT: return L"SIDE_LEFT";
	case eSIDE_RIGHT: return L"SIDE_RIGHT";
	case eAUTO: return L"AUTO";
	default: return L"unknown_channel";
	}
}

bool audio_analyzer::operator<(Channel left, Channel right) {
	return left.value < right.value;
}


sview ChannelLayout::getName() const {
	return name;
}

std::optional<index> ChannelLayout::indexOf(Channel channel) const {
	const auto iter = channelMap.find(channel);
	if (iter == channelMap.end()) {
		return std::nullopt;
	}
	return iter->second;
}

bool ChannelLayout::contains(Channel channel) const {
	return channelMap.find(channel) != channelMap.end();
}

const std::vector<Channel>& ChannelLayout::getChannelsOrderView() const {
	return channelOrder;
}

const std::unordered_map<Channel, index>& ChannelLayout::getChannelsMapView() const {
	return channelMap;
}

LayoutBuilder& LayoutBuilder::add(Channel channel) {
	layout.channelMap[channel] = nextIndex;
	nextIndex++;

	return *this;
}

ChannelLayout LayoutBuilder::finish() const {
	return layout;
}

ChannelLayout ChannelLayouts::getMono() {
	return mono;
}

ChannelLayout ChannelLayouts::getStereo() {
	return stereo;
}

ChannelLayout ChannelLayouts::layoutFromChannelMask(uint32_t mask, bool forceBackSpeakers) {
	switch (mask) {
	case KSAUDIO_SPEAKER_MONO:				return mono;
	case KSAUDIO_SPEAKER_1POINT1:			return _1_1;
	case KSAUDIO_SPEAKER_STEREO:			return stereo;
	case KSAUDIO_SPEAKER_2POINT1:			return _2_1;
	case KSAUDIO_SPEAKER_3POINT0:			return _3_0;
	case KSAUDIO_SPEAKER_3POINT1:			return _3_1;
	case KSAUDIO_SPEAKER_QUAD:				return quad;
	case KSAUDIO_SPEAKER_5POINT0:			return _5_0;
	case KSAUDIO_SPEAKER_5POINT1:			return _5_1;
	case KSAUDIO_SPEAKER_7POINT0:			return _7_0;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND:	return _7_1surround;
	default:;
	}

	if (mask == KSAUDIO_SPEAKER_5POINT1_SURROUND) {
		if (forceBackSpeakers) {
			return _5_1;
		} else {
			return _5_1_surround;
		}
	}

	struct {
		uint32_t channelBit;
		std::optional<Channel> channelOpt;
	} speakers[] = {
		{ SPEAKER_FRONT_LEFT, Channel::eFRONT_LEFT },
		{ SPEAKER_FRONT_RIGHT, Channel::eFRONT_RIGHT},
		{ SPEAKER_FRONT_CENTER, Channel::eCENTER},
		{ SPEAKER_LOW_FREQUENCY, Channel::eLOW_FREQUENCY},
		{ SPEAKER_BACK_LEFT, Channel::eBACK_LEFT},
		{ SPEAKER_BACK_RIGHT, Channel::eBACK_RIGHT},
		{ SPEAKER_FRONT_LEFT_OF_CENTER, std::nullopt },
		{ SPEAKER_FRONT_RIGHT_OF_CENTER, std::nullopt },
		{ SPEAKER_BACK_CENTER, Channel::eCENTER_BACK },
		{ SPEAKER_SIDE_LEFT, Channel::eSIDE_LEFT},
		{ SPEAKER_SIDE_RIGHT, Channel::eSIDE_RIGHT},
		{ SPEAKER_TOP_CENTER, std::nullopt },
		{ SPEAKER_TOP_FRONT_LEFT, std::nullopt },
		{ SPEAKER_TOP_FRONT_CENTER, std::nullopt },
		{ SPEAKER_TOP_FRONT_RIGHT, std::nullopt },
		{ SPEAKER_TOP_BACK_LEFT, std::nullopt },
		{ SPEAKER_TOP_BACK_CENTER, std::nullopt },
		{ SPEAKER_TOP_BACK_RIGHT, std::nullopt },
	};

	LayoutBuilder builder;
	for (auto[bit, channelOpt] : speakers) {
		if ((mask & bit) == 0) {
			continue;
		}
		if (channelOpt.has_value()) {
			builder.add(channelOpt.value());
		} else {
			builder.skip();
		}
	}
	// TODO also need to add code to determine (from skin side) if a channel exist in layout

	return builder.finish();
}
