/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ImageTransposer.h"

#include "undef.h"

void utils::ImageTransposer::transposeToBuffer(const Vector2D<uint32_t>& imageData, index lineOffset) {
	const auto sourceWidth = imageData.getBufferSize();
	const auto sourceHeight = imageData.getBuffersCount();
	buffer.setBufferSize(sourceHeight);
	buffer.setBuffersCount(sourceWidth);

	for (index sourceY = lineOffset; sourceY < sourceHeight; sourceY++) {
		transposeLine(sourceY - lineOffset, imageData[sourceY]);
	}

	for (index sourceY = 0; sourceY < lineOffset; sourceY++) {
		transposeLine(sourceY + sourceHeight - lineOffset, imageData[sourceY]);
	}
}

const utils::Vector2D<unsigned>& utils::ImageTransposer::getBuffer() const {
	return buffer;
}

void utils::ImageTransposer::transposeLine(index lineIndex, array_view<uint32_t> lineData) {
	for (index x = 0; x < lineData.size(); x++) {
		buffer[x][lineIndex] = lineData[x];
	}
}
