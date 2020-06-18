/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HelperClasses.h"

#include "undef.h"

using namespace audio_analyzer;


DataSupplierImpl::DataSupplierImpl(utils::Vector2D<float> &wave) {
	this->wave = &wave;
}

void DataSupplierImpl::setChannelData(const ChannelData *value) {
	channelData = value;
}

void DataSupplierImpl::setChannelIndex(index channelIndex) {
	this->channelIndex = channelIndex;
}

void DataSupplierImpl::setChannel(Channel channel) {
	this->channel = channel;
}

const float* DataSupplierImpl::getWave() const {
	return (*wave)[channelIndex].data();
}

index DataSupplierImpl::getWaveSize() const {
	return waveSize;
}

const SoundHandler* DataSupplierImpl::getHandlerRaw(isview id) const {
	const auto iter = channelData->indexMap.find(id);
	if (iter == channelData->indexMap.end()) {
		return nullptr;
	}
	auto handler = channelData->handlers[iter->second].get();
	handler->finish(*this); // endless loop is impossible because of "source" checking in ParamParser

	if (!handler->isValid()) {
		return nullptr;
	}

	return handler;
}

Channel DataSupplierImpl::getChannel() const {
	return channel;
}

std::byte* DataSupplierImpl::getBufferRaw(index size) const {
	if (nextBufferIndex >= index(buffers.size())) {
		buffers.emplace_back();
	}
	auto &buffer = buffers[nextBufferIndex];
	nextBufferIndex++;
	buffer.resize(size);
	return buffer.data();
}

void DataSupplierImpl::resetBuffers() {
	nextBufferIndex = 0;
}

void DataSupplierImpl::setWaveSize(index value) {
	waveSize = value;
}
