/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveFormDrawer.h"

#include "undef.h"

using namespace utils;

WaveFormDrawer::WaveFormDrawer() {
	inflatableBuffer.setBackground(0.0);
}

void WaveFormDrawer::setDimensions(index width, index height) {
	inflatableBuffer.setDimensions(width, height);
	resultBuffer.setBufferSize(width);
	resultBuffer.setBuffersCount(height);
	stripBuffer.resize(height);

	interpolator = { -1.0, 1.0, 0, height - 1 };

	this->width = width;
	this->height = height;
}

void WaveFormDrawer::fillSilence() {
	inflatableBuffer.pushEmptyLine(0.0);
}

void WaveFormDrawer::fillStrip(double min, double max) {
	fillStripBuffer(min, max);
	inflatableBuffer.pushStrip(stripBuffer);
}

void WaveFormDrawer::inflate() {
	auto imageLines = inflatableBuffer.getPixels();

	for (int lineIndex = 0; lineIndex < height; ++lineIndex) {
		auto source = imageLines[lineIndex];
		auto dest = resultBuffer[lineIndex];

		for (int i = 0; i < width; ++i) {
			const auto sourceValue = source[i];
			const auto color = Color::mix(1.0 - sourceValue, colors.background, colors.wave);
			dest[i] = color.toInt();
		}
	}

	const index centerLineIndex = interpolator.toValueD(0.0);
	auto sourceCenter = imageLines[centerLineIndex];
	auto destCenter = resultBuffer[centerLineIndex];

	if (lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		const auto color = colors.line.toInt();
		for (int i = 0; i < width; ++i) {
			destCenter[i] = color;
		}
	} else if (lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
		for (int i = 0; i < width; ++i) {
			destCenter[i] = Color::mix(1.0 - sourceCenter[i], colors.line, colors.wave).toInt();
		}
	}
}
