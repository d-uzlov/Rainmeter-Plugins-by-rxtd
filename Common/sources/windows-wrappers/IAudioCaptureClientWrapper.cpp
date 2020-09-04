/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "IAudioCaptureClientWrapper.h"
#include <type_traits>

using namespace utils;

static_assert(std::is_same<BYTE, uint8_t>::value);
static_assert(std::is_same<UINT32, uint32_t>::value);

// static_assert(std::is_same<DWORD, uint32_t>::value); // ...

void IAudioCaptureClientWrapper::readBuffer() {
	uint8_t* data = nullptr;
	DWORD flags{ };
	uint32_t dataSize;
	lastResult = ref().GetBuffer(&data, &dataSize, &flags, nullptr, nullptr);

	if (lastResult != S_OK || dataSize == 0) {
		return;
	}

	const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

	buffer.setBuffersCount(channelsCount);
	buffer.setBufferSize(dataSize);

	if (silent) {
		buffer.fill(0.0);
	} else {
		switch (type) {
		case Type::eInt16:
			for (index i = 0; i < channelsCount; i++) {
				copyInt(data, buffer[i], i, channelsCount);
			}
			break;
		case Type::eFloat:
			for (index i = 0; i < channelsCount; i++) {
				copyFloat(data, buffer[i], i, channelsCount);
			}
			break;
		}
	}

	ref().ReleaseBuffer(dataSize);
}

void IAudioCaptureClientWrapper::copyFloat(void* source, array_span<float> dest, index offset, index stride) {
	const auto bufferInt = static_cast<const float*>(source);
	const auto framesCount = dest.size();

	auto channelSourceBuffer = bufferInt + offset;

	for (index frame = 0; frame < framesCount; ++frame) {
		dest[frame] = *channelSourceBuffer;
		channelSourceBuffer += stride;
	}
}

void IAudioCaptureClientWrapper::copyInt(void* source, array_span<float> dest, index offset, index stride) {
	const auto bufferInt = static_cast<const int16_t*>(source);
	const auto framesCount = dest.size();

	auto channelSourceBuffer = bufferInt + offset;

	for (index frame = 0; frame < framesCount; ++frame) {
		const float value = float(*channelSourceBuffer) * (1.0f / std::numeric_limits<int16_t>::max());
		dest[frame] = value;
		channelSourceBuffer += stride;
	}
}
