/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TransposedStripedImageHelper.h"
#include "BmpWriter.h"

#include "undef.h"

using namespace utils;

void TransposedStripedImageHelper::setBackground(PixelColor value) {
	backgroundValue = value;
}

void TransposedStripedImageHelper::setDimensions(index width, index height) {
	if (this->width == width && this->height == height) {
		return;
	}
	this->width = width;
	this->height = height;

	beginningOffset = 0;

	const index vectorSize = width * height;
	maxOffset = getReserveSize(vectorSize);

	pixelData.reserve(vectorSize + maxOffset);

	auto imageLines = getCurrentLinesArray();
	imageLines.init(backgroundValue);

	lastFillValue = backgroundValue;
	sameStripsCount = imageLines.getBuffersCount();
	emptinessWritten = false;
}

void TransposedStripedImageHelper::pushStrip(array_view<PixelColor> stripData) {
	sameStripsCount = 0;
	emptinessWritten = false;

	incrementStrip();

	auto imageLines = getCurrentLinesArray();
	const index lastStripIndex = width - 1;
	for (index i = 0; i < stripData.size(); i++) {
		imageLines[i][lastStripIndex] = stripData[i];
	}
}

void TransposedStripedImageHelper::pushEmptyLine(PixelColor value) {
	if (sameStripsCount >= width) {
		return;
	}

	if (sameStripsCount == 0 || lastFillValue != value) {
		lastFillValue = value;
		sameStripsCount = 1;
	} else {
		sameStripsCount++;
	}
	emptinessWritten = false;

	incrementStrip();

	auto imageLines = getCurrentLinesArray();
	const index lastStripIndex = width - 1;
	for (index i = 0; i < height; i++) {
		imageLines[i][lastStripIndex] = value;
	}
}

void TransposedStripedImageHelper::write(const string& filepath) const {
	if (emptinessWritten) {
		return;
	}

	BmpWriter::writeFile(filepath, getCurrentLinesArray());

	if (isEmpty()) {
		emptinessWritten = true;
	}
}

index TransposedStripedImageHelper::getReserveSize(index size) {
	constexpr double reserveCoef = 0.5;
	return static_cast<index>(std::ceil(size * reserveCoef));
}

array2d_span<TransposedStripedImageHelper::PixelColor> TransposedStripedImageHelper::getCurrentLinesArray() {
	return { pixelData.data() + beginningOffset, height, width };
}

array2d_view<TransposedStripedImageHelper::PixelColor> TransposedStripedImageHelper::getCurrentLinesArray() const {
	return { pixelData.data() + beginningOffset, height, width };
}

void TransposedStripedImageHelper::incrementStrip() {
	if (beginningOffset < maxOffset) {
		beginningOffset++;
		return;
	}

	std::copy(
		pixelData.data() + beginningOffset + 1,
		pixelData.data() + beginningOffset + width * height,
		pixelData.data()
	);
	beginningOffset = 0;
}
