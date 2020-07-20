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

std::variant<SoundHandler*, AudioChildHelper::SearchError>
AudioChildHelper::findHandler(Channel channel, isview handlerId) const {
	const auto channelIter = channels->find(channel);
	if (channelIter == channels->end()) {
		return SearchError::eCHANNEL_NOT_FOUND;
	}

	const auto& channelData = channelIter->second;
	const auto iter = channelData.indexMap.find(handlerId);
	if (iter == channelData.indexMap.end()) {
		return SearchError::eHANDLER_NOT_FOUND;
	}

	auto& handler = channelData.handlers[iter->second];
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

std::variant<const wchar_t*, AudioChildHelper::SearchError>
AudioChildHelper::getProp(Channel channel, sview handlerId, sview prop) const {
	const auto handlerVariant = findHandler(channel, handlerId % ciView());
	if (handlerVariant.index() != 0) {
		return std::get<1>(handlerVariant);
	}

	auto& handler = std::get<0>(handlerVariant);
	return handler->getProp(prop % ciView());
}
