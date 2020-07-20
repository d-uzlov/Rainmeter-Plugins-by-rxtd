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

std::pair<double, double> WaveFormDrawer::correctMinMaxPixels(double minPixel, double maxPixel) const {
	const auto minMaxDelta = maxPixel - minPixel;
	if (minMaxDelta < 1) {
		const auto average = (minPixel + maxPixel) * 0.5;
		minPixel = average - 0.5;
		maxPixel = minPixel + 1.0; // max should always be >= min + 1

		const auto minD = interpolator.makeDiscreet(minPixel);
		const auto minDC = interpolator.makeDiscreetClamped(minPixel);
		if (minD != minDC) {
			const double shift = 1.0 - interpolator.percentRelativeToNext(minPixel);
			minPixel += shift;
			maxPixel += shift;
		}

		const auto maxD = interpolator.makeDiscreet(maxPixel);
		const auto maxDC = interpolator.makeDiscreetClamped(maxPixel);
		if (maxD != maxDC) {
			const double shift = -interpolator.percentRelativeToNext(maxPixel);
			minPixel += shift;
			maxPixel += shift;
		}
	}

	return { minPixel, maxPixel };
}

void WaveFormDrawer::fillStripBuffer(double min, double max) {
	auto& buffer = stripBuffer;

	min = std::clamp(min, -1.0, 1.0);
	max = std::clamp(max, -1.0, 1.0);
	min = std::min(min, max);
	max = std::max(min, max);

	const auto [minPixel, maxPixel] = correctMinMaxPixels(interpolator.toValue(min), interpolator.toValue(max));

	const auto lowBackgroundBound = interpolator.makeDiscreetClamped(minPixel);
	const auto highBackgroundBound = interpolator.makeDiscreetClamped(maxPixel);

	// Should never really clamp due to minPixel <= maxPixel - 1
	// Because discreet(maxPixel) and discreet(minPixel) shouldn't clamp after shift above
	// If (max - min) was already >= 1.0, then also no clamp because discreet(toValue([-1.0, 1.0])) should be within clamp range
	const auto lowLineBound = interpolator.clamp(lowBackgroundBound + 1);
	const auto highLineBound = highBackgroundBound;

	// [0, lowBackgroundBound) ← background
	// [lowBackgroundBound, lowBackgroundBound] ← transition
	// [lowBackgroundBound + 1, highBackgroundBound) ← line
	// [highBackgroundBound, highBackgroundBound] ← transition
	// [highBackgroundBound + 1, MAX] ← background

	for (index i = 0; i < lowBackgroundBound; ++i) {
		buffer[i] = 0.0;
	}

	const double lowPercent = interpolator.percentRelativeToNext(minPixel);
	if (edgeAntialiasing) {
		buffer[lowBackgroundBound] = 1.0 - lowPercent;
	} else {
		if (lowPercent < 0.5) {
			buffer[lowBackgroundBound] = 1.0;
		} else {
			buffer[lowBackgroundBound] = 0.0;
		}
	}

	for (index i = lowLineBound; i < highLineBound; i++) {
		buffer[i] = 1.0;
	}

	const double highPercent = interpolator.percentRelativeToNext(maxPixel);
	if (edgeAntialiasing) {
		buffer[highBackgroundBound] = highPercent;
	} else {
		if (highPercent > 0.5) {
			buffer[highBackgroundBound] = 1.0;
		} else {
			buffer[highBackgroundBound] = 0.0;
		}
	}

	for (index i = highBackgroundBound + 1; i < height; ++i) {
		buffer[i] = 0.0;
	}
}
