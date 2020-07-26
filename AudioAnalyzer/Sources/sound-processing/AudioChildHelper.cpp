/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioChildHelper.h"

#include "undef.h"

using namespace audio_analyzer;


AudioChildHelper::AudioChildHelper(std::map<Channel, ChannelData>& channels, DataSupplierImpl& dataSupplier) {
	this->channels = &channels;
	this->dataSupplier = &dataSupplier;
}

std::variant<SoundHandler*, AudioChildHelper::SearchResult>
AudioChildHelper::findHandler(Channel channel, isview handlerId) const {
	const auto channelIter = channels->find(channel);
	if (channelIter == channels->end()) {
		return SearchResult::eCHANNEL_NOT_FOUND;
	}

	const auto& channelData = channelIter->second;
	const auto iter = channelData.find(handlerId % own()); // TODO move to sview
	if (iter == channelData.end()) {
		return SearchResult::eHANDLER_NOT_FOUND;
	}

	auto& handler = iter->second;
	return handler.get();
}

double AudioChildHelper::getValue(Channel channel, isview handlerId, index index) const {
	const auto handlerVariant = findHandler(channel, handlerId);
	if (handlerVariant.index() != 0) {
		return 0.0;
	}

	auto& handler = std::get<0>(handlerVariant);

	const auto channelDataIter = channels->find(channel);
	if (channelDataIter == channels->end()) {
		return 0.0;
	}
	dataSupplier->setChannelData(&channelDataIter->second);

	handler->finish(*dataSupplier);
	if (!handler->isValid()) {
		return 0.0;
	}

	const auto layersCount = handler->getLayersCount();
	if (layersCount <= 0) {
		return 0.0;
	}

	const auto data = handler->getData(0);
	if (data.empty()) {
		return 0.0;
	}
	if (index >= data.size()) {
		return 0.0;
	}
	return data[index];
}

double AudioChildHelper::getValueFrom(SoundHandler* handler, Channel channel, index index) const {
	const auto channelDataIter = channels->find(channel);
	if (channelDataIter == channels->end()) {
		return 0.0;
	}
	dataSupplier->setChannelData(&channelDataIter->second);

	handler->finish(*dataSupplier);
	if (!handler->isValid()) {
		return 0.0;
	}

	const auto layersCount = handler->getLayersCount();
	if (layersCount <= 0) {
		return 0.0;
	}

	const auto data = handler->getData(0);
	if (data.empty()) {
		return 0.0;
	}
	if (index >= data.size()) {
		return 0.0;
	}
	return data[index];
}
