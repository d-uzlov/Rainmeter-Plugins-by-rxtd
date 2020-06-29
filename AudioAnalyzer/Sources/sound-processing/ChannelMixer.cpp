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

	resampler.setSourceRate(waveFormat.samplesPerSec);

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
		aliasOfAuto = waveFormat.channelLayout.getChannelsOrderView()[0];
	}
}

void ChannelMixer::decomposeFramesIntoChannels(const uint8_t* buffer, index framesCount, bool withAuto) {
	const auto channelsCount = waveFormat.channelsCount;

	if (waveFormat.format == utils::WaveDataFormat::ePCM_F32) {
		const auto bufferFloat = reinterpret_cast<const float*>(buffer);
		
		for (auto channel : waveFormat.channelLayout) {
			channels[channel].resampled = false;
			auto &waveBuffer = channels[channel].wave;
			waveBuffer.resize(framesCount);
			auto bufferTemp = bufferFloat + waveFormat.channelLayout.indexOf(channel).value();

			for (index frame = 0; frame < framesCount; ++frame) {
				waveBuffer[frame] = *bufferTemp;
				bufferTemp += channelsCount;
			}
		}
	} else if (waveFormat.format == utils::WaveDataFormat::ePCM_S16) {
		const auto bufferInt = reinterpret_cast<const int16_t*>(buffer);

		for (auto channel : waveFormat.channelLayout) {
			channels[channel].resampled = false;
			auto &waveBuffer = channels[channel].wave;
			waveBuffer.resize(framesCount);
			auto bufferTemp = bufferInt + waveFormat.channelLayout.indexOf(channel).value();

			for (index frame = 0; frame < framesCount; ++frame) {
				waveBuffer[frame] = *bufferTemp * (1.0f / std::numeric_limits<int16_t>::max());
				bufferTemp += channelsCount;
			}
		}
	} else {
		std::terminate(); // TODO this is dumb
	}

	if (withAuto && aliasOfAuto == Channel::eAUTO) {
		resampleToAuto();
	}
}

array_view<float> ChannelMixer::getChannelPCM(Channel channel) {
	if (channel == Channel::eAUTO) {
		channel = aliasOfAuto;
	}

	const auto dataIter = channels.find(channel);
	if (dataIter == channels.end()) {
		return { };
	}

	auto& data = channels[channel];
	if (!data.resampled) {
		resampler.resample(data.wave);
		data.wave.resize(resampler.calculateFinalWaveSize(data.wave.size()));
	}

	return data.wave;
}

Resampler& ChannelMixer::getResampler() {
	return resampler;
}

void ChannelMixer::resampleToAuto() {
	auto& bufferAuto = channels[Channel::eAUTO].wave;
	const auto& bufferFirst = channels[Channel::eFRONT_LEFT].wave;
	const auto& bufferSecond = channels[Channel::eFRONT_RIGHT].wave;

	const index size = bufferFirst.size();
	bufferAuto.resize(size);
	for (index i = 0; i < size; ++i) {
		bufferAuto[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}
