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

	const index centerLineIndex = interpolator.toValueD(0.0);
	inflatableBuffer.correctLastLine(centerLineIndex, 1.0);
}

void WaveFormDrawer::fillStrip(double min, double max) {
	min = std::clamp(min, -1.0, 1.0);
	max = std::clamp(max, -1.0, 1.0);
	min = std::min(min, max);
	max = std::max(min, max);

	if (connected) {
		min = std::min(min, prev.max);
		max = std::max(max, prev.min);

		prev.min = min;
		prev.max = max;
	}

	fillStripBuffer(min, max);
	inflatableBuffer.pushStrip(stripBuffer);
}

void WaveFormDrawer::inflate() {
	auto imageLines = inflatableBuffer.getPixels();

	for (int lineIndex = 0; lineIndex < height; ++lineIndex) {
		auto source = imageLines[lineIndex];
		auto dest = resultBuffer[lineIndex];

		inflateLine(source, dest);
	}

	const index centerLineIndex = interpolator.toValueD(0.0);
	auto sourceCenter = imageLines[centerLineIndex];
	auto destCenter = resultBuffer[centerLineIndex];

	if (lineDrawingPolicy == LineDrawingPolicy::eALWAYS) {
		const auto color = colors.line.toInt();
		for (int i = 0; i < width; ++i) {
			destCenter[i] = color;
		}
	} else if (lineDrawingPolicy == LineDrawingPolicy::eBELOW_WAVE) {
		for (int i = 0; i < width; ++i) {
			destCenter[i] = Color::mix(1.0 - sourceCenter[i], colors.line, colors.wave).toInt();
		}
	}
}

void WaveFormDrawer::inflateLineNoFade(array_view<float> source, array_span<uint32_t> dest) {
	const index lastStripIndex = inflatableBuffer.getLastStripIndex();

	const double realWidth = width - borderSize;
	for (int i = 0; i < width; ++i) {
		double distance = lastStripIndex - i;
		if (distance < 0.0) {
			distance += width;
		}
		if (distance >= realWidth) {
			// this is border
			dest[i] = colors.border.toInt();
			continue;
		}

		dest[i] = inflatePixel(source, 0.0, i);
	}
}

void WaveFormDrawer::inflateLinePow1(array_view<float> source, array_span<uint32_t> dest) {
	const index lastStripIndex = inflatableBuffer.getLastStripIndex();
	const double realWidth = width - borderSize;
	const double distanceCoef = 1.0 / realWidth;

	for (int i = 0; i < width; ++i) {
		double distance = lastStripIndex - i;
		if (distance < 0.0) {
			distance += width;
		}
		if (distance >= realWidth) {
			// this is border
			dest[i] = colors.border.toInt();
			continue;
		}

		distance *= distanceCoef;

		dest[i] = inflatePixel(source, distance, i);
	}
}

void WaveFormDrawer::inflateLinePow2(array_view<float> source, array_span<uint32_t> dest) {
	const index lastStripIndex = inflatableBuffer.getLastStripIndex();
	const double realWidth = width - borderSize;
	const double distanceCoef = 1.0 / realWidth;

	float prev = 0.0;

	for (int i = 0; i < width; ++i) {
		double distance = lastStripIndex - i;
		if (distance < 0.0) {
			distance += width;
		}
		if (distance >= realWidth) {
			// this is border
			dest[i] = colors.border.toInt();
			continue;
		}

		distance *= distanceCoef;
		distance = distance * distance;

		dest[i] = inflatePixel(source, distance, i);
	}
}

void WaveFormDrawer::inflateLinePow4(array_view<float> source, array_span<uint32_t> dest) {
	const index lastStripIndex = inflatableBuffer.getLastStripIndex();
	const double realWidth = width - borderSize;
	const double distanceCoef = 1.0 / realWidth;

	for (int i = 0; i < width; ++i) {
		double distance = lastStripIndex - i;
		if (distance < 0.0) {
			distance += width;
		}
		if (distance >= realWidth) {
			// this is border
			dest[i] = colors.border.toInt();
			continue;
		}

		distance *= distanceCoef;
		distance = distance * distance;
		distance = distance * distance;

		dest[i] = inflatePixel(source, distance, i);
	}
}

void WaveFormDrawer::inflateLinePow8(array_view<float> source, array_span<uint32_t> dest) {
	const index lastStripIndex = inflatableBuffer.getLastStripIndex();
	const double realWidth = width - borderSize;
	const double distanceCoef = 1.0 / realWidth;

	for (int i = 0; i < width; ++i) {
		double distance = lastStripIndex - i;
		if (distance < 0.0) {
			distance += width;
		}
		if (distance >= realWidth) {
			// this is border
			dest[i] = colors.border.toInt();
			continue;
		}

		distance *= distanceCoef;
		distance = distance * distance;
		distance = distance * distance;
		distance = distance * distance;

		dest[i] = inflatePixel(source, distance, i);
	}
}

uint32_t WaveFormDrawer::inflatePixel(array_view<float> source, double backgroundCoef, index i) {
	const float current = source[i];
	const index nextIndex = i < width - 2 ? i + 1 : 0;
	const float neighbours = std::max(inflatingPrev, source[nextIndex]);
	inflatingPrev = current;

	if (current > 2.0) { // when peak
		inflatingPrev = current - 2.0;
		const auto mixedColor = Color::mix(backgroundCoef, colors.background, colors.halo);
		return mixedColor.toInt();
	}

	if (edges == SmoothEdges::eHALO && neighbours > current * 2.0) {
		const auto sourceValue = (current + neighbours) * 0.5 * (1.0 - backgroundCoef);
		const auto mixedColor = Color::mix(1.0 - sourceValue, colors.background, colors.halo);
		return mixedColor.toInt();
	}

	// generic
	const auto sourceValue = current * (1.0 - backgroundCoef);
	const auto mixedColor = Color::mix(1.0 - sourceValue, colors.background, colors.wave);
	return mixedColor.toInt();
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

	for (index i = lowLineBound; i < highLineBound; i++) {
		buffer[i] = 1.0;
	}

	for (index i = highBackgroundBound + 1; i < height; ++i) {
		buffer[i] = 0.0;
	}

	const double lowPercent = interpolator.percentRelativeToNext(minPixel);
	const double highPercent = interpolator.percentRelativeToNext(maxPixel);

	switch (edges) {
	case SmoothEdges::eNONE: {
		if (lowPercent < 0.5) {
			buffer[lowBackgroundBound] = 1.0;
		} else {
			buffer[lowBackgroundBound] = 0.0;
		}

		if (highPercent > 0.5) {
			buffer[highBackgroundBound] = 1.0;
		} else {
			buffer[highBackgroundBound] = 0.0;
		}
		break;
	}
	case SmoothEdges::eMIN_MAX: {
		buffer[lowBackgroundBound] = 1.0 - lowPercent;
		buffer[highBackgroundBound] = highPercent;
		break;
	}
	case SmoothEdges::eHALO: {
		buffer[lowBackgroundBound] = 1.0 - lowPercent + 2.0;
		buffer[highBackgroundBound] = highPercent + 2.0;
		break;
	}
	default: ;
	}
}
