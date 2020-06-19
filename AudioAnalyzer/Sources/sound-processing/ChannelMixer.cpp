/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ChannelMixer.h"

#include "undef.h"
#include "Channel.h"

using namespace audio_analyzer;

void ChannelMixer::setFormat(MyWaveFormat waveFormat) {
	this->waveFormat = waveFormat;
}

void ChannelMixer::setBuffer(utils::Vector2D<float> &buffer) {
	this->bufferPtr = &buffer;
}

void ChannelMixer::decomposeFramesIntoChannels(const uint8_t* buffer, index framesCount) {
	auto& waveBuffer = *bufferPtr;

	waveBuffer.setBufferSize(framesCount);

	const auto channelsCount = waveFormat.channelsCount;
	if (waveFormat.format == Format::ePCM_F32) {
		auto s = reinterpret_cast<const float*>(buffer);
		for (index frame = 0; frame < framesCount; ++frame) {
			for (index channel = 0; channel < channelsCount; ++channel) {
				waveBuffer[channel][frame] = *s;
				++s;
			}
		}
	} else if (waveFormat.format == Format::ePCM_S16) {
		auto s = reinterpret_cast<const int16_t*>(buffer);
		for (index frame = 0; frame < framesCount; ++frame) {
			for (index channel = 0; channel < channelsCount; ++channel) {
				waveBuffer[channel][frame] = *s * (1.0f / std::numeric_limits<int16_t>::max());
				++s;
			}
		}
	} else {
		std::terminate(); // TODO this is dumb
	}
}

index ChannelMixer::createChannelAuto(index framesCount, index destinationChannel) {

	auto left = waveFormat.channelLayout.indexOf(Channel::eFRONT_LEFT);
	auto right = waveFormat.channelLayout.indexOf(Channel::eFRONT_RIGHT);

	if (left.has_value() && right.has_value()) {
		resampleToAuto(left.value(), right.value(), framesCount, destinationChannel);
		return destinationChannel;
	}
	if (left.has_value()) {
		copyToAuto(left.value(), framesCount, destinationChannel);
		return destinationChannel;
	}
	if (right.has_value()) {
		copyToAuto(right.value(), framesCount, destinationChannel);
		return destinationChannel;
	}

	auto center = waveFormat.channelLayout.indexOf(Channel::eCENTER);
	if (center.has_value()) {
		copyToAuto(center.value(), framesCount, destinationChannel);
		return destinationChannel;
	}

	copyToAuto(0, framesCount, destinationChannel);
	return destinationChannel;
}

void ChannelMixer::resampleToAuto(index first, index second, index framesCount, index destinationChannel) {
	auto& buffer = *bufferPtr;

	auto bufferAuto = buffer[destinationChannel];
	const auto bufferFirst = buffer[first];
	const auto bufferSecond = buffer[second];

	for (index i = 0; i < framesCount; ++i) {
		bufferAuto[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}

void ChannelMixer::copyToAuto(index channelIndex, index framesCount, index destinationChannel) {
	auto& buffer = *bufferPtr;

	auto bufferAuto = buffer[destinationChannel];
	const auto bufferSource = buffer[channelIndex];

	for (index i = 0; i < framesCount; ++i) {
		bufferAuto[i] = bufferSource[i];
	}
}
