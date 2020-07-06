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

using namespace utils;

void LinedImageHelper::setBackground(Color value) {
	backgroundValue = value;
	transposer.setBackground(value);
}

void LinedImageHelper::setImageWidth(index width) {
	if (imageLines.getBufferSize() == width) {
		return;
	}

	imageLines.setBufferSize(width);
	imageLines.init(backgroundValue);

	lastFillValue = backgroundValue;
	sameLinesCount = imageLines.getBuffersCount();
}

index LinedImageHelper::getImageWidth() const {
	return imageLines.getBufferSize();
}

void LinedImageHelper::setImageHeight(index height) {
	if (imageLines.getBuffersCount() == height) {
		return;
	}

	imageLines.setBuffersCount(height);
	imageLines.init(backgroundValue);

	lastFillValue = backgroundValue;
	sameLinesCount = imageLines.getBuffersCount();
}

index LinedImageHelper::getImageHeight() const {
	return imageLines.getBuffersCount();
}

array_span<Color> LinedImageHelper::nextLine() {
	sameLinesCount = 0;
	return nextLineNonBreaking();
}

void LinedImageHelper::fillNextLineFlat(Color value) {
	if (sameLinesCount == 0 || lastFillValue != value) {
		lastFillValue = value;
		sameLinesCount = 1;
	} else {
		if (sameLinesCount >= imageLines.getBuffersCount()) {
			return;
		}
		sameLinesCount++;
	}

	auto line = nextLineNonBreaking();
	std::fill(line.begin(), line.end(), value);
}

array_span<Color> LinedImageHelper::fillNextLineManual() {
	if (sameLinesCount < imageLines.getBuffersCount()) {
		sameLinesCount++;
	}

	return nextLineNonBreaking();
}

void LinedImageHelper::writeTransposed(const string& filepath, bool withOffset, bool withFading) const {
	if (isEmpty()) {
		return;
	}

	index offset = lastLineIndex + 1;
	if (offset >= imageLines.getBuffersCount()) {
		offset = 0;
	}
	index gradientOffset = 0;
	if (!withOffset) {
		std::swap(offset, gradientOffset);
	}

	const auto width = imageLines.getBufferSize();
	const auto height = imageLines.getBuffersCount();
	transposer.transposeToBuffer(imageLines, offset, withFading, gradientOffset);
	
	BmpWriter::writeFile(filepath, transposer.getBuffer()[0].data(), width, height);
}

bool LinedImageHelper::isEmpty() const {
	return sameLinesCount > imageLines.getBuffersCount();
}

void LinedImageHelper::collapseInto(array_span<Color> result) const {
	std::fill(result.begin(), result.end(), Color{ 0.0, 0.0, 0.0, 0.0 });

	for (int lineIndex = 0; lineIndex < imageLines.getBuffersCount(); lineIndex++) {
		const auto line = imageLines[lineIndex];
		for (int i = 0; i < line.size(); ++i) {
			result[i] = result[i] + line[i];
		}
	}

	const float coef = 1.0 / imageLines.getBuffersCount();
	for (int i = 0; i < imageLines.getBufferSize(); ++i) {
		result[i] = result[i] * coef;
	}
}

array_span<Color> LinedImageHelper::nextLineNonBreaking() {
	lastLineIndex++;
	if (lastLineIndex >= imageLines.getBuffersCount()) {
		lastLineIndex = 0;
	}
	return imageLines[lastLineIndex];
}
