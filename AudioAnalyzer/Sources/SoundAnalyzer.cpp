/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SoundAnalyzer.h"
#include <limits>

#include "undef.h"

using namespace audio_analyzer;

SoundAnalyzer::DataSupplierImpl::DataSupplierImpl(SoundAnalyzer& parent) : parent(parent) { }

void SoundAnalyzer::DataSupplierImpl::setChannelData(const ChannelData *value) {
	channelData = value;
}

void SoundAnalyzer::DataSupplierImpl::setChannelIndex(index channelIndex) {
	this->channelIndex = channelIndex;
}

void SoundAnalyzer::DataSupplierImpl::setChannel(Channel channel) {
	this->channel = channel;
}

const float* SoundAnalyzer::DataSupplierImpl::getWave() const {
	return parent.wave[channelIndex].data();
}

index SoundAnalyzer::DataSupplierImpl::getWaveSize() const {
	return waveSize;
}

const SoundHandler* SoundAnalyzer::DataSupplierImpl::getHandlerRaw(isview id) const {
	const auto iter = channelData->indexMap.find(id);
	if (iter == channelData->indexMap.end()) {
		return nullptr;
	}
	auto handler = channelData->handlers[iter->second].get();
	handler->finish(*this); // TODO check for endless loop

	if (!handler->isValid()) {
		return nullptr;
	}

	return handler;
}

Channel SoundAnalyzer::DataSupplierImpl::getChannel() const {
	return channel;
}

std::byte* SoundAnalyzer::DataSupplierImpl::getBufferRaw(index size) const {
	if (nextBufferIndex >= index(buffers.size())) {
		buffers.emplace_back();
	}
	auto &buffer = buffers[nextBufferIndex];
	nextBufferIndex++;
	buffer.resize(size);
	return buffer.data();
}

void SoundAnalyzer::DataSupplierImpl::resetBuffers() {
	nextBufferIndex = 0;
}

void SoundAnalyzer::DataSupplierImpl::setWaveSize(index value) {
	waveSize = value;
}

SoundAnalyzer::SoundAnalyzer() noexcept : dataSupplier(*this) {

}

void SoundAnalyzer::decompose(const uint8_t* buffer, index framesCount) noexcept {
	wave.setBufferSize(framesCount);

	const auto channelsCount = waveFormat.channelsCount;
	if (waveFormat.format == Format::ePCM_F32) {
		auto s = reinterpret_cast<const float*>(buffer);
		for (index frame = 0; frame < framesCount; ++frame) {
			for (index channel = 0; channel < channelsCount; ++channel) {
				wave[channel][frame] = *s;
				++s;
			}
		}
	} else if (waveFormat.format == Format::ePCM_S16) {
		auto s = reinterpret_cast<const int16_t*>(buffer);
		for (index frame = 0; frame < framesCount; ++frame) {
			for (index channel = 0; channel < channelsCount; ++channel) {
				wave[channel][frame] = *s * (1.0f / std::numeric_limits<int16_t>::max());
				++s;
			}
		}
	} else {
		std::terminate();
	}
}

void SoundAnalyzer::resample(array_span<float> values, index framesCount) const noexcept {
	if (divide <= 1) {
		return;
	}
	const auto newCount = framesCount / divide;
	for (index i = 0; i < newCount; ++i) {
		double value = 0.0;
		for (index j = 0; j < divide; ++j) {
			value += values[i * divide + j];
		}
		values[i] = static_cast<float>(value / divide);
	}
}

void SoundAnalyzer::updateSampleRate() noexcept {
	if (waveFormat.format == Format::eINVALID) {
		return;
	}

	if (targetRate == 0) {
		divide = 1;
	} else {
		const auto ratio = static_cast<double>(waveFormat.samplesPerSec) / targetRate;
		if (ratio > 1) {
			divide = static_cast<decltype(divide)>(ratio);
		} else {
			divide = 1;
		}
	}
	const auto sampleRate = waveFormat.samplesPerSec / divide;
	for (auto & channel : channels) {
		for (auto &handler : channel.second.handlers) {
			handler->setSamplesPerSec(sampleRate);
		}
	}
}

void SoundAnalyzer::setTargetRate(index value) noexcept {
	targetRate = value;
	updateSampleRate();
}

std::variant<SoundHandler*, SoundAnalyzer::SearchError>
SoundAnalyzer::findHandler(Channel channel, isview handlerId) const noexcept {
	const auto channelIter = channels.find(channel);
	if (channelIter == channels.end()) {
		return SearchError::eCHANNEL_NOT_FOUND;
	}

	const auto &channelData = channelIter->second;
	const auto iter = channelData.indexMap.find(handlerId);
	if (iter == channelData.indexMap.end()) {
		return SearchError::eHANDLER_NOT_FOUND;
	}

	auto &handler = channelData.handlers[iter->second];
	return handler.get();
}

double SoundAnalyzer::getValue(Channel channel, isview handlerId, index index) const noexcept {
	const auto handlerVariant = findHandler(channel, handlerId);
	if (handlerVariant.index() != 0) {
		return 0.0;
	}

	auto &handler = std::get<0>(handlerVariant);

	const auto channelDataIter = channels.find(channel);
	if (channelDataIter == channels.end()) {
		return 0.0;
	}
	dataSupplier.setChannelData(&channelDataIter->second);
	dataSupplier.setChannel(channel);

	handler->finish(dataSupplier);
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

std::variant<const wchar_t*, SoundAnalyzer::SearchError>
SoundAnalyzer::getProp(Channel channel, sview handlerId, sview prop) const noexcept {
	const auto handlerVariant = findHandler(channel, handlerId % ciView());
	if (handlerVariant.index() != 0) {
		return std::get<1>(handlerVariant);
	}

	auto &handler = std::get<0>(handlerVariant);
	return handler->getProp(prop % ciView());
}

void SoundAnalyzer::setPatchHandlers(std::map<Channel, std::vector<istring>> handlersOrder,
	std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap) noexcept {

	for (const auto &channelOrder : handlersOrder) {
		auto channelIter = channels.find(channelOrder.first);
		if (channelIter == channels.end()) {
			// channel specified in options but don't exist in the layout
			continue;
		}

		decltype(ChannelData::handlers) newHandlers;
		decltype(ChannelData::indexMap) newIndexMap;

		auto &channelData = channelIter->second;
		index index = 0;

		for (auto &handlerName : channelOrder.second) {
			auto iter3 = handlerPatchersMap.find(handlerName);
			if (iter3 == handlerPatchersMap.end()) {
				continue;
			}

			std::unique_ptr<SoundHandler> handler;

			auto iter2 = channelData.indexMap.find(handlerName);
			if (iter2 == channelData.indexMap.end()) {
				handler = std::unique_ptr<SoundHandler>(iter3->second(nullptr));
			} else {
				std::unique_ptr<SoundHandler> &oldHandler = channelData.handlers[iter2->second];
				SoundHandler* res = iter3->second(oldHandler.get());
				if (res != oldHandler.get()) {
					oldHandler.reset();
					handler = std::unique_ptr<SoundHandler>(res);
				} else {
					handler = std::move(oldHandler);
				}
			}

			newHandlers.push_back(std::move(handler));
			newIndexMap[handlerName] = index;
			index++;
		}

		channelData.handlers = std::move(newHandlers);
		channelData.indexMap = std::move(newIndexMap);
	}

	this->patchers = std::move(handlerPatchersMap);
	this->orderOfHandlers = std::move(handlersOrder);

	updateSampleRate();
}

void SoundAnalyzer::setWaveFormat(MyWaveFormat waveFormat) noexcept {
	if (waveFormat.format == Format::eINVALID) {
		this->waveFormat = waveFormat;
		return;
	}

	wave.setBuffersCount(waveFormat.channelsCount + 1ll);

	// remove channels that no longer exist
	std::vector<Channel> toDelete;
	for (const auto &channelIter : channels) {
		Channel c = channelIter.first;

		if (c == Channel::eAUTO) {
			continue;
		}

		if (!waveFormat.channelLayout.contains(c)) {
			toDelete.push_back(c);
		}
	}
	for (auto c : toDelete) {
		channels.erase(c);
	}

	auto channelLayoutMap = waveFormat.channelLayout.getChannelsView();
	channelLayoutMap[Channel::eAUTO] = -1;

	// add channels that didn't exist
	for (auto [newChannel, _] : channelLayoutMap) {
		// skip already existing channels
		if (channels.find(newChannel) != channels.end()) {
			continue;
		}

		// create channel data before checking existing of handlers for this channel
		// data should always exist, even when empty
		auto &data = channels[newChannel];

		// keep channel empty if no handlers specified
		const auto iterOrder = orderOfHandlers.find(newChannel);
		if (iterOrder == orderOfHandlers.end()) {
			continue;
		}

		// Fill channelData with handlers
		index index = 0;
		for (const auto &handlerIndex : iterOrder->second) {
			const auto iterPatcher = patchers.find(handlerIndex);
			if (iterPatcher == patchers.end() || iterPatcher->second == nullptr) {
				continue;
			}

			SoundHandler* soundHandler = iterPatcher->second(nullptr);
			data.handlers.push_back(std::unique_ptr<SoundHandler>(soundHandler));
			data.indexMap[handlerIndex] = index;
			index++;
		}
	}

	this->waveFormat = waveFormat;
	updateSampleRate();
}

void SoundAnalyzer::process(const uint8_t* buffer, bool isSilent, index framesCount) noexcept {
	if (waveFormat.format == Format::eINVALID || waveFormat.channelsCount <= 0) {
		return;
	}

	if (!isSilent) {
		decompose(buffer, framesCount);
	}

	dataSupplier.setWaveSize(framesCount / divide);

	dataSupplier.setChannelData(&channels[Channel::eAUTO]);
	dataSupplier.setChannel(Channel::eAUTO);

	auto &autoHandlers = channels[Channel::eAUTO].handlers;
	if (!autoHandlers.empty()) {
		// must be processed before other channels because wave is destroyed after resample()
		if (isSilent) {
			for (auto& handler : autoHandlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			// data.handlers always not empty
			const auto waveIndex = createChannelAuto(framesCount);
			if (waveIndex >= 0) {
				dataSupplier.setChannelIndex(waveIndex);
				resample(wave[waveIndex], framesCount);

				for (auto& handler : autoHandlers) {
					handler->process(dataSupplier);
				}
			}
		}
		dataSupplier.resetBuffers();
	}

	for (auto &channelOrderIter : orderOfHandlers) {
		const auto channel = channelOrderIter.first;
		const auto dataIter = channels.find(channel);
		if (dataIter == channels.end()) {
			continue;
		}

		auto &data = dataIter->second;

		dataSupplier.setChannelData(&data);
		dataSupplier.setChannel(channel);

		const std::optional<index> waveIndexOpt = waveFormat.channelLayout.fromChannel(channel);
		if (!waveIndexOpt.has_value()) {
			continue;
		}
		const index waveIndex = waveIndexOpt.value();

		if (waveIndex < 0 || waveIndex >= waveFormat.channelsCount) {
			continue;
		}

		dataSupplier.setChannelIndex(waveIndex);

		if (isSilent) {
			for (auto& handler : data.handlers) {
				handler->processSilence(dataSupplier);
			}
		} else {
			// data.handlers always not empty
			resample(wave[waveIndex], framesCount);

			for (auto& handler : data.handlers) {
				handler->process(dataSupplier);
			}
		}
		dataSupplier.resetBuffers();
	}

	dataSupplier.setChannelData(nullptr);
}

void SoundAnalyzer::resetValues() noexcept {
	for (auto& [_, channel] : channels) {
		for (auto& handler : channel.handlers) {
			handler->reset();
		}
	}
}

void SoundAnalyzer::finish() noexcept {
	for (auto&[_, channel] : channels) {
		for (auto& handler : channel.handlers) {
			if (handler->isStandalone()) {
				handler->finish(dataSupplier);
			}
		}
	}
}

index SoundAnalyzer::createChannelAuto(index framesCount) noexcept {
	auto iter = orderOfHandlers.find(Channel::eAUTO);
	if (iter == orderOfHandlers.end()) {
		return -1;
	}
	auto autoChannelOrderList = iter->second;
	if (autoChannelOrderList.empty()) {
		return -1;
	}

	const index channelIndex = wave.getBuffersCount() - 1;

	auto left = waveFormat.channelLayout.fromChannel(Channel::eFRONT_LEFT);
	auto right = waveFormat.channelLayout.fromChannel(Channel::eFRONT_RIGHT);

	if (left.has_value() && right.has_value()) {
		resampleToAuto(left.value(), right.value(), framesCount);
		return channelIndex;
	}
	if (left.has_value()) {
		copyToAuto(left.value(), framesCount);
		return channelIndex;
	}
	if (right.has_value()) {
		copyToAuto(right.value(), framesCount);
		return channelIndex;
	}

	auto center = waveFormat.channelLayout.fromChannel(Channel::eCENTER);
	if (center.has_value()) {
		copyToAuto(center.value(), framesCount);
		return channelIndex;
	}

	copyToAuto(0, framesCount);
	return channelIndex;
}

void SoundAnalyzer::resampleToAuto(index first, index second, index framesCount) noexcept {
	auto bufferAuto = wave[wave.getBuffersCount() - 1];
	const auto bufferFirst = wave[first];
	const auto bufferSecond = wave[second];

	for (index i = 0; i < framesCount; ++i) {
		bufferAuto[i] = (bufferFirst[i] + bufferSecond[i]) * 0.5f;
	}
}

void SoundAnalyzer::copyToAuto(index channelIndex, index framesCount) noexcept {
	auto bufferAuto = wave[wave.getBuffersCount() - 1];
	const auto bufferSource = wave[channelIndex];

	for (index i = 0; i < framesCount; ++i) {
		bufferAuto[i] = bufferSource[i];
	}
}
