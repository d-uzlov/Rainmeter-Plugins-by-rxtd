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

using namespace audio_analyzer;

namespace {
	ChannelLayout mono = {
		L"1.0 mono", {
			Channel::eCENTER
		}
	};
	ChannelLayout _1_1 = {
		L"1.1", {
			Channel::eCENTER, Channel::eLOW_FREQUENCY
		}
	};
	ChannelLayout stereo = {
		L"2.0 stereo", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT
		}
	};
	ChannelLayout _2_1 = {
		L"2.1", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eLOW_FREQUENCY
		}
	};
	ChannelLayout _3_0 = {
		L"3.0", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER
		}
	};
	ChannelLayout _3_1 = {
		L"3.1", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER, Channel::eLOW_FREQUENCY
		}
	};
	ChannelLayout quad = {
		L"4.0 quad", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eBACK_LEFT, Channel::eBACK_RIGHT
		}
	};
	ChannelLayout quad_surround = {
		L"4.0 surround", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER, Channel::eCENTER_BACK
		}
	};
	ChannelLayout _5_0 = {
		L"5.0", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER,
			Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		}
	};
	ChannelLayout _5_1 = {
		L"5.1", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER, Channel::eLOW_FREQUENCY,
			Channel::eBACK_LEFT, Channel::eBACK_RIGHT
		}
	};
	ChannelLayout _5_1_surround = {
		L"5.1 surround", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER, Channel::eLOW_FREQUENCY,
			Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		}
	};
	ChannelLayout _7_0 = {
		L"7.0", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER,
			Channel::eBACK_LEFT, Channel::eBACK_RIGHT,
			Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		}
	};
	ChannelLayout _7_1surround = {
		L"7.1 surround", {
			Channel::eFRONT_LEFT, Channel::eFRONT_RIGHT,
			Channel::eCENTER, Channel::eLOW_FREQUENCY,
			Channel::eBACK_LEFT, Channel::eBACK_RIGHT,
			Channel::eSIDE_LEFT, Channel::eSIDE_RIGHT
		}
	};
}

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

	return { };
}

sview ChannelUtils::getTechnicalName(Channel channel) {
	switch (channel) {
	case Channel::eFRONT_LEFT: return L"FRONT_LEFT";
	case Channel::eFRONT_RIGHT: return L"FRONT_RIGHT";
	case Channel::eCENTER: return L"CENTER";
	case Channel::eCENTER_BACK: return L"CENTER_BACK";
	case Channel::eLOW_FREQUENCY: return L"LOW_FREQUENCY";
	case Channel::eBACK_LEFT: return L"BACK_LEFT";
	case Channel::eBACK_RIGHT: return L"BACK_RIGHT";
	case Channel::eSIDE_LEFT: return L"SIDE_LEFT";
	case Channel::eSIDE_RIGHT: return L"SIDE_RIGHT";
	case Channel::eAUTO: return L"AUTO";
	}
	return { };
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

std::optional<index> ChannelLayout::indexOf(Channel channel) const {
	const auto iter = channelMap.find(channel);
	if (iter == channelMap.end()) {
		return std::nullopt;
	}
	return iter->second;
}


ChannelLayout ChannelUtils::parseLayout(uint32_t bitMask, bool forceBackSpeakers) {
	switch (bitMask) {
	case KSAUDIO_SPEAKER_MONO: return mono;
	case KSAUDIO_SPEAKER_1POINT1: return _1_1;
	case KSAUDIO_SPEAKER_STEREO: return stereo;
	case KSAUDIO_SPEAKER_2POINT1: return _2_1;
	case KSAUDIO_SPEAKER_3POINT0: return _3_0;
	case KSAUDIO_SPEAKER_3POINT1: return _3_1;
	case KSAUDIO_SPEAKER_QUAD: return quad;
	case KSAUDIO_SPEAKER_SURROUND: return quad_surround;
	case KSAUDIO_SPEAKER_5POINT0: return _5_0;
	case KSAUDIO_SPEAKER_5POINT1: return _5_1;
	case KSAUDIO_SPEAKER_7POINT0: return _7_0;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND: return _7_1surround;
	default: ;
	}

	if (bitMask == KSAUDIO_SPEAKER_5POINT1_SURROUND) {
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
	for (auto [bit, channelOpt] : speakers) {
		if ((bitMask & bit) == 0) {
			continue;
		}
		channels.push_back(channelOpt);
	}

	return { L"???", std::move(channels) };
}
