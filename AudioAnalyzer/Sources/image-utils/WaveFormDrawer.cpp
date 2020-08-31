/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveFormDrawer.h"

#include "IntMixer.h"

using namespace utils;

void WaveFormDrawer::setImageParams(index width, index height, bool stationary) {
	if (width == this->width && height == this->height) {
		return;
	}

	minMaxBuffer.setParams(width, 1, { 0, 0 }, stationary);
	resultBuffer.setBufferSize(width);
	resultBuffer.setBuffersCount(height);

	interpolator = { -1.0, 1.0, 0, height - 1 };

	this->width = width;
	this->height = height;

	const index centerLineIndex = interpolator.toValueD(0.0);
	prev.minPixel = centerLineIndex;
	prev.maxPixel = centerLineIndex;
}

void WaveFormDrawer::fillSilence() {
	const index centerLineIndex = interpolator.toValueD(0.0);
	minMaxBuffer.pushEmptyStrip({ centerLineIndex, centerLineIndex });
}

void WaveFormDrawer::fillStrip(double min, double max) {
	min = std::clamp(min, -1.0, 1.0);
	max = std::clamp(max, -1.0, 1.0);
	min = std::min(min, max);
	max = std::max(min, max);

	auto minPixel = interpolator.toValueD(min);
	auto maxPixel = interpolator.toValueD(max);

	if (connected) {
		minPixel = std::min(minPixel, prev.maxPixel);
		maxPixel = std::max(maxPixel, prev.minPixel);

		prev.minPixel = minPixel;
		prev.maxPixel = maxPixel;
	}

	if (minPixel == maxPixel) {
		if (minPixel > 0) {
			minPixel--;
		} else {
			maxPixel++;
		}
	}

	MinMax mm{ minPixel, maxPixel };
	minMaxBuffer.pushStrip({ &mm, 1 });
}

void WaveFormDrawer::inflate() {
	const index centerLineIndex = interpolator.toValueD(0.0);

	for (index lineIndex = 0; lineIndex < centerLineIndex; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
	}

	for (index lineIndex = centerLineIndex + 1; lineIndex < height; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
	}

	if (lineDrawingPolicy == LineDrawingPolicy::eALWAYS) {
		auto destCenter = resultBuffer[centerLineIndex];
		for (index i = 0; i < width; ++i) {
			destCenter[i] = colors.line;
		}
	} else if (lineDrawingPolicy == LineDrawingPolicy::eBELOW_WAVE) {
		inflateLine(centerLineIndex, resultBuffer[centerLineIndex], colors.line);
	} else {
		inflateLine(centerLineIndex, resultBuffer[centerLineIndex], colors.background);
	}
}

void WaveFormDrawer::inflateLine(index line, array_span<IntColor> dest, IntColor backgroundColor) const {
	const index realWidth = width - borderSize;

	const index fadeWidth = index(realWidth * fading);
	const index flatWidth = realWidth - fadeWidth;

	constexpr uint32_t halfPrecision = 8;
	IntMixer<int_fast32_t, halfPrecision * 2> mixer;

	int_fast32_t fadeDistance = 1 << halfPrecision;
	const int_fast32_t fadeDistanceStep = int_fast32_t(std::round(fadeDistance / (realWidth * fading)));

	index fadeBeginIndex = minMaxBuffer.getPastLastStripIndex() + borderSize;
	if (fadeBeginIndex >= width) {
		fadeBeginIndex -= width;
	}

	index flatBeginIndex = fadeBeginIndex + fadeWidth;

	if (flatBeginIndex >= width) {
		for (index i = fadeBeginIndex; i < width; i++) {
			mixer.setFactorWarped(fadeDistance * fadeDistance);
			const auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
			dest[i] = backgroundColor.mixWith(sc, mixer);

			fadeDistance -= fadeDistanceStep;
		}

		fadeBeginIndex = 0;
		flatBeginIndex -= width;
	}

	for (index i = fadeBeginIndex; i < flatBeginIndex; i++) {
		mixer.setFactorWarped(fadeDistance * fadeDistance);
		const auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
		dest[i] = backgroundColor.mixWith(sc, mixer);

		fadeDistance -= fadeDistanceStep;
	}

	index borderBeginIndex = flatBeginIndex + flatWidth;
	if (borderBeginIndex >= width) {
		for (index i = flatBeginIndex; i < width; i++) {
			auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
			dest[i] = sc;
		}

		flatBeginIndex = 0;
		borderBeginIndex -= width;
	}

	for (index i = flatBeginIndex; i < borderBeginIndex; i++) {
		auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
		dest[i] = sc;
	}

	const auto border = colors.border;
	index borderEndIndex = borderBeginIndex + borderSize;
	if (borderEndIndex >= width) {
		for (index i = borderBeginIndex; i < width; i++) {
			dest[i] = border;
		}
		borderBeginIndex = 0;
		borderEndIndex -= width;
	}
	for (index i = borderBeginIndex; i < borderEndIndex; i++) {
		dest[i] = border;
	}
}

bool WaveFormDrawer::isWaveAt(index i, index line) const {
	const auto minMax = minMaxBuffer.getPixels()[0][i];
	return line >= minMax.minPixel && line <= minMax.maxPixel;
}
