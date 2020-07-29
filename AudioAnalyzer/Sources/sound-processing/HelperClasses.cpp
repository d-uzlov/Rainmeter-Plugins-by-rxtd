/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HelperClasses.h"

using namespace audio_analyzer;

void DataSupplierImpl::setWave(array_view<float> value) {
	wave = value;
}

void DataSupplierImpl::setChannelData(const ChannelData* value) {
	channelData = value;
}

array_view<float> DataSupplierImpl::getWave() const {
	return wave;
}

const SoundHandler* DataSupplierImpl::getHandlerRaw(isview id) const {
	const auto iter = channelData->find(id);
	if (iter == channelData->end()) {
		return nullptr;
	}
	auto handler = iter->second.get();
	handler->finish(); // endless loop is impossible because of "source" checking in ParamParser

	if (!handler->isValid()) {
		return nullptr;
	}

	return handler;
}

std::byte* DataSupplierImpl::getBufferRaw(index size) const {
	if (nextBufferIndex >= index(buffers.size())) {
		buffers.emplace_back();
	}
	auto& buffer = buffers[nextBufferIndex];
	nextBufferIndex++;
	buffer.resize(size);
	return buffer.data();
}

void DataSupplierImpl::resetBuffers() {
	nextBufferIndex = 0;
}
