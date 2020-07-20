/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "StripedImage.h"

#include "undef.h"

using namespace utils;

void StripedImage::setBackground(PixelColor value) {
	backgroundValue = value;
}

void StripedImage::setDimensions(index width, index height) {
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
}

void StripedImage::pushStrip(array_view<PixelColor> stripData) {
	sameStripsCount = 0;

	incrementStrip();

	auto imageLines = getCurrentLinesArray();
	const index lastStripIndex = width - 1;
	for (index i = 0; i < stripData.size(); i++) {
		imageLines[i][lastStripIndex] = stripData[i];
	}
}

void StripedImage::pushEmptyLine(PixelColor value) {
	if (sameStripsCount >= width) {
		return;
	}

	if (sameStripsCount == 0 || lastFillValue != value) {
		lastFillValue = value;
		sameStripsCount = 1;
	} else {
		sameStripsCount++;
	}

	incrementStrip();

	auto imageLines = getCurrentLinesArray();
	const index lastStripIndex = width - 1;
	for (index i = 0; i < height; i++) {
		imageLines[i][lastStripIndex] = value;
	}
}

void StripedImage::pushEmptyStrip(array_view<PixelColor> stripData) {
	if (sameStripsCount >= width) {
		return;
	}

	if (sameStripsCount == 0) {
		sameStripsCount = 1;
	} else {
		sameStripsCount++;
	}

	incrementStrip();

	auto imageLines = getCurrentLinesArray();
	const index lastStripIndex = width - 1;
	for (index i = 0; i < stripData.size(); i++) {
		imageLines[i][lastStripIndex] = stripData[i];
	}
}

index StripedImage::getReserveSize(index size) {
	constexpr double reserveCoef = 0.5;
	return static_cast<index>(std::ceil(size * reserveCoef));
}

array2d_span<StripedImage::PixelColor> StripedImage::getCurrentLinesArray() {
	return { pixelData.data() + beginningOffset, height, width };
}

array2d_view<StripedImage::PixelColor> StripedImage::getCurrentLinesArray() const {
	return { pixelData.data() + beginningOffset, height, width };
}

void StripedImage::incrementStrip() {
	if (beginningOffset < maxOffset) {
		beginningOffset++;
		return;
	}

	std::copy(
		pixelData.data() + maxOffset + 1, // this +1 is very important. Without it image won't shift enough
		pixelData.data() + maxOffset + width * height,
		pixelData.data()
	);
	beginningOffset = 0;
}
