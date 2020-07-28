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

#include "undef.h"
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
		aliasOfAuto = waveFormat.channelLayout.getChannelsOrderView()[0]; // todo when empty there will be a crash
	}

	std::vector<Channel> toDelete;
	for (const auto&[channel, _] : channels) {
		const bool exists = channel == Channel::eAUTO || waveFormat.channelLayout.contains(channel);
		if (!exists) {
			toDelete.push_back(channel);
		}
	}
	for (auto c : toDelete) {
		channels.erase(c);
	}

	// Create missing channels
	for (const auto channel : waveFormat.channelLayout) {
		channels[channel];
	}
	channels[aliasOfAuto];
}

void ChannelMixer::decomposeFramesIntoChannels(array_view<std::byte> frameBuffer, bool withAuto) {
	const auto channelsCount = waveFormat.channelsCount;
	const auto framesCount = frameBuffer.size();

	if (waveFormat.format == utils::WaveDataFormat::ePCM_F32) {
		const auto bufferFloat = reinterpret_cast<const float*>(frameBuffer.data());

		for (auto channel : waveFormat.channelLayout) {
			auto& waveBuffer = channels[channel];
			waveBuffer.resize(framesCount);
			auto bufferTemp = bufferFloat + waveFormat.channelLayout.indexOf(channel).value();

			for (index frame = 0; frame < framesCount; ++frame) {
				waveBuffer[frame] = *bufferTemp;
				bufferTemp += channelsCount;
			}
		}
	} else if (waveFormat.format == utils::WaveDataFormat::ePCM_S16) {
		const auto bufferInt = reinterpret_cast<const int16_t*>(frameBuffer.data());

		for (auto channel : waveFormat.channelLayout) {
			auto& waveBuffer = channels[channel];
			waveBuffer.resize(framesCount);
			auto bufferTemp = bufferInt + waveFormat.channelLayout.indexOf(channel).value();

			for (index frame = 0; frame < framesCount; ++frame) {
				// I don't own anything that produces ePCM_S16 format, so I can't test if it needs same correcting coefficient as ePCM_F32
				const float value = *bufferTemp * (1.0f / std::numeric_limits<int16_t>::max());
				waveBuffer[frame] = value;
				bufferTemp += channelsCount;
			}
		}
	} else {
		// If a format is unknown then there should be
		// no wave data in the first place
		// so this function should never be called
		std::terminate();
	}

	if (withAuto && aliasOfAuto == Channel::eAUTO) {
		resampleToAuto();
	}

	isSilent = false;
}

void ChannelMixer::writeSilence(index size, bool withAuto) {
	if (isSilent) {
		return;
	}

	for (auto& [channel, buffer] : channels) {
		buffer.resize(size);
		std::fill(buffer.begin(), buffer.end(), 0.0);
	}

	if (withAuto && aliasOfAuto == Channel::eAUTO) {
		resampleToAuto();
	}

	isSilent = true;

}

array_view<float> ChannelMixer::getChannelPCM(Channel channel) const {
	if (channel == Channel::eAUTO) {
		channel = aliasOfAuto;
	}

	const auto dataIter = channels.find(channel);
	if (dataIter == channels.end()) {
		return { };
	}

	return dataIter->second;
}

void ChannelMixer::resampleToAuto() {
	auto& bufferAuto = channels[Channel::eAUTO];
	const auto& bufferFirst = channels[Channel::eFRONT_LEFT];
	const auto& bufferSecond = channels[Channel::eFRONT_RIGHT];

	const index size = bufferFirst.size();
	bufferAuto.resize(size);
	for (index i = 0; i < size; ++i) {
		bufferAuto[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}
