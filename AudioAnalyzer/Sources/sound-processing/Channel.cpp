/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Channel.h"
#include <my-windows.h>
#include <Audioclient.h>

using namespace rxtd::audio_analyzer;

std::optional<Channel> ChannelUtils::parse(isview string) {
	if (string == L"Auto") {
		return Channel::eAUTO;
	}
	if (string == L"Left" || string == L"FrontLeft" || string == L"FL") {
		return Channel::eFRONT_LEFT;
	}
	if (string == L"Right" || string == L"FrontRight" || string == L"FR") {
		return Channel::eFRONT_RIGHT;
	}
	if (string == L"Center" || string == L"C") {
		return Channel::eCENTER;
	}
	if (string == L"CenterBack" || string == L"CB") {
		return Channel::eCENTER_BACK;
	}
	if (string == L"LowFrequency" || string == L"LFE") {
		return Channel::eLOW_FREQUENCY;
	}
	if (string == L"BackLeft" || string == L"BL") {
		return Channel::eBACK_LEFT;
	}
	if (string == L"BackRight" || string == L"BR") {
		return Channel::eBACK_RIGHT;
	}
	if (string == L"SideLeft" || string == L"SL") {
		return Channel::eSIDE_LEFT;
	}
	if (string == L"SideRight" || string == L"SR") {
		return Channel::eSIDE_RIGHT;
	}

	return {};
}

rxtd::sview ChannelUtils::getTechnicalName(Channel channel) {
	switch (channel) {
	case Channel::eFRONT_LEFT: return L"fl";
	case Channel::eFRONT_RIGHT: return L"fr";
	case Channel::eCENTER: return L"c";
	case Channel::eCENTER_BACK: return L"cb";
	case Channel::eLOW_FREQUENCY: return L"lfe";
	case Channel::eBACK_LEFT: return L"bl";
	case Channel::eBACK_RIGHT: return L"br";
	case Channel::eSIDE_LEFT: return L"sl";
	case Channel::eSIDE_RIGHT: return L"sr";
	case Channel::eAUTO: return L"a";
	}
	return {};
}

rxtd::sview ChannelUtils::getHumanName(Channel channel) {
	switch (channel) {
	case Channel::eFRONT_LEFT: return L"FrontLeft";
	case Channel::eFRONT_RIGHT: return L"FrontRight";
	case Channel::eCENTER: return L"Center";
	case Channel::eCENTER_BACK: return L"CenterBack";
	case Channel::eLOW_FREQUENCY: return L"LFE";
	case Channel::eBACK_LEFT: return L"BackLeft";
	case Channel::eBACK_RIGHT: return L"BackRight";
	case Channel::eSIDE_LEFT: return L"SideLeft";
	case Channel::eSIDE_RIGHT: return L"SideRight";
	case Channel::eAUTO: return L"Auto";
	}
	return {};
}


ChannelLayout::ChannelLayout(sview _name, std::vector<std::optional<Channel>> channels) {
	name = _name;

	for (index i = 0; i < static_cast<index>(channels.size()); i++) {
		auto channelOpt = channels[i];
		if (!channelOpt.has_value()) {
			continue;
		}

		auto channel = channelOpt.value();
		channelMap[channel] = i;
		channelOrder.push_back(channel);
	}
}

std::optional<rxtd::index> ChannelLayout::indexOf(Channel channel) const {
	const auto iter = channelMap.find(channel);
	if (iter == channelMap.end()) {
		return std::nullopt;
	}
	return iter->second;
}

rxtd::sview getLayoutName(uint32_t bitMask) {
	switch (bitMask) {
	case KSAUDIO_SPEAKER_MONO: return L"1.0 mono";
	case KSAUDIO_SPEAKER_1POINT1: return L"1.1";
	case KSAUDIO_SPEAKER_STEREO: return L"2.0 stereo";
	case KSAUDIO_SPEAKER_2POINT1: return L"2.1";
	case KSAUDIO_SPEAKER_3POINT0: return L"3.0";
	case KSAUDIO_SPEAKER_3POINT1: return L"3.1";
	case KSAUDIO_SPEAKER_QUAD: return L"4.0 quad";
	case KSAUDIO_SPEAKER_SURROUND: return L"4.0 surround";
	case KSAUDIO_SPEAKER_5POINT0: return L"5.0";
	case KSAUDIO_SPEAKER_5POINT1: return L"5.1";
	case KSAUDIO_SPEAKER_5POINT1_SURROUND: return L"5.1 surround";
	case KSAUDIO_SPEAKER_7POINT0: return L"7.0";
	case KSAUDIO_SPEAKER_7POINT1_SURROUND: return L"7.1 surround";
	default: break;
	}

	return L"";
}

ChannelLayout ChannelUtils::parseLayout(uint32_t bitMask, bool forbid5Point1Surround) {
	if (bitMask == KSAUDIO_SPEAKER_5POINT1_SURROUND && forbid5Point1Surround) {
		bitMask = KSAUDIO_SPEAKER_5POINT1;
	}

	struct {
		uint32_t channelBit;
		std::optional<Channel> channelOpt;
	} speakersBitMasks[] = {
			{ SPEAKER_FRONT_LEFT, Channel::eFRONT_LEFT },
			{ SPEAKER_FRONT_RIGHT, Channel::eFRONT_RIGHT },
			{ SPEAKER_FRONT_CENTER, Channel::eCENTER },
			{ SPEAKER_LOW_FREQUENCY, Channel::eLOW_FREQUENCY },
			{ SPEAKER_BACK_LEFT, Channel::eBACK_LEFT },
			{ SPEAKER_BACK_RIGHT, Channel::eBACK_RIGHT },
			{ SPEAKER_FRONT_LEFT_OF_CENTER, std::nullopt },
			{ SPEAKER_FRONT_RIGHT_OF_CENTER, std::nullopt },
			{ SPEAKER_BACK_CENTER, Channel::eCENTER_BACK },
			{ SPEAKER_SIDE_LEFT, Channel::eSIDE_LEFT },
			{ SPEAKER_SIDE_RIGHT, Channel::eSIDE_RIGHT },
			{ SPEAKER_TOP_CENTER, std::nullopt },
			{ SPEAKER_TOP_FRONT_LEFT, std::nullopt },
			{ SPEAKER_TOP_FRONT_CENTER, std::nullopt },
			{ SPEAKER_TOP_FRONT_RIGHT, std::nullopt },
			{ SPEAKER_TOP_BACK_LEFT, std::nullopt },
			{ SPEAKER_TOP_BACK_CENTER, std::nullopt },
			{ SPEAKER_TOP_BACK_RIGHT, std::nullopt },
		};

	std::vector<std::optional<Channel>> channels;
	for (auto [bit, channelOpt] : speakersBitMasks) {
		if ((bitMask & bit) == 0) {
			continue;
		}
		channels.push_back(channelOpt);
	}

	const sview name = getLayoutName(bitMask);
	return { name, std::move(channels) };
}
