/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioChildHelper.h"

using namespace audio_analyzer;

AudioChildHelper::AudioChildHelper(const std::map<Channel, ChannelData>& channels) {
	this->channels = &channels;
}

SoundHandler* AudioChildHelper::findHandler(Channel channel, isview handlerId) const {
	const auto channelIter = channels->find(channel);
	if (channelIter == channels->end()) {
		return { };
	}

	const auto& channelData = channelIter->second;
	const auto iter = channelData.find(handlerId);
	if (iter == channelData.end()) {
		return { };
	}

	auto& handler = iter->second;
	return handler.ptr.get();
}

double AudioChildHelper::getValue(Channel channel, isview handlerId, index index) const {
	const auto handler = findHandler(channel, handlerId);
	if (handler == nullptr) {
		return 0.0;
	}

	return getValueFrom(handler, channel, index);
}

double AudioChildHelper::getValueFrom(SoundHandler* handler, Channel channel, index index) const {
	const auto channelDataIter = channels->find(channel);
	if (channelDataIter == channels->end()) {
		return 0.0;
	}

	const auto layersCount = handler->getDataSize().layersCount;

	if (layersCount <= 0) {
		return 0.0;
	}

	handler->finish();
	if (!handler->isValid()) {
		return 0.0;
	}

	const auto values = handler->getData().values[0];
	if (values.empty()) {
		return 0.0;
	}
	if (index >= values.size()) {
		return 0.0;
	}
	return values[index];
}
