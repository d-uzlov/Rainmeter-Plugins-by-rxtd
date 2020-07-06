/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "LinedImageHelper.h"
#include "BmpWriter.h"

#include "undef.h"

void utils::LinedImageHelper::setBackground(uint32_t value) {
	backgroundValue = value;
}

void utils::LinedImageHelper::setImageWidth(index width) {
	if (buffer.getBufferSize() == width) {
		return;
	}

	buffer.setBufferSize(width);
	buffer.init(backgroundValue);

	lastFillValue = backgroundValue;
	sameLinesCount = buffer.getBuffersCount();
}

void utils::LinedImageHelper::setImageHeight(index height) {
	if (buffer.getBuffersCount() == height) {
		return;
	}

	buffer.setBuffersCount(height);
	buffer.init(backgroundValue);

	lastFillValue = backgroundValue;
	sameLinesCount = buffer.getBuffersCount();
}

array_span<uint32_t> utils::LinedImageHelper::nextLine() {
	sameLinesCount = 0;
	return nextLineNonBreaking();
}

void utils::LinedImageHelper::fillNextLineFlat(uint32_t value) {
	if (sameLinesCount == 0 || lastFillValue != value) {
		lastFillValue = value;
		sameLinesCount = 1;
	} else {
		if (sameLinesCount >= buffer.getBuffersCount()) {
			return;
		}
		sameLinesCount++;
	}

	auto line = nextLineNonBreaking();
	std::fill(line.begin(), line.end(), value);
}

array_span<uint32_t> utils::LinedImageHelper::fillNextLineManual() {
	if (sameLinesCount < buffer.getBuffersCount()) {
		sameLinesCount++;
	}

	return nextLineNonBreaking();
}

void utils::LinedImageHelper::writeTransposed(const string& filepath) const {
	if (sameLinesCount >= buffer.getBuffersCount()) {
		return;
	}

	auto index = lastLineIndex + 1;
	if (index >= buffer.getBuffersCount()) {
		index = 0;
	}

	const auto width = buffer.getBufferSize();
	const auto height = buffer.getBuffersCount();
	const auto writeBufferSize = width * height;
	writeBuffer.resize(writeBufferSize);
	BmpWriter::writeFile(filepath, buffer[0].data(), width, height, index, writeBuffer);
}

array_span<uint32_t> utils::LinedImageHelper::nextLineNonBreaking() {
	lastLineIndex++;
	if (lastLineIndex >= buffer.getBuffersCount()) {
		lastLineIndex = 0;
	}
	return buffer[lastLineIndex];
}
