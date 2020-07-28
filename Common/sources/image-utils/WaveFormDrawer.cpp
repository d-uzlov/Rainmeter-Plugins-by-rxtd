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
#include "IntMixer.h"

using namespace utils;

WaveFormDrawer::WaveFormDrawer() {
	minMaxBuffer.setBackground({ 0, 0 });
}

void WaveFormDrawer::setDimensions(index width, index height) {
	if (width == this->width && height == this->height) {
		return;
	}

	minMaxBuffer.setDimensions(width, 1);
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
	minMaxBuffer.pushEmptyStrip({ centerLineIndex, centerLineIndex + 1 });
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

	minMaxBuffer.pushEmptyStrip({ minPixel, maxPixel });
}

void WaveFormDrawer::inflate() {
	const index centerLineIndex = interpolator.toValueD(0.0);

	for (int lineIndex = 0; lineIndex < centerLineIndex; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
	}

	for (int lineIndex = centerLineIndex + 1; lineIndex < height; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
	}

	if (lineDrawingPolicy == LineDrawingPolicy::eALWAYS) {
		auto destCenter = resultBuffer[centerLineIndex];
		for (int i = 0; i < width; ++i) {
			destCenter[i] = colors.line.full;
		}
	} else if (lineDrawingPolicy == LineDrawingPolicy::eBELOW_WAVE) {
		inflateLine(centerLineIndex, resultBuffer[centerLineIndex], colors.line);
	}
}

void WaveFormDrawer::inflateLine(index line, array_span<uint32_t> dest, IntColor backgroundColor) const {
	const double realWidth = width - borderSize;

	const index fadeWidth = realWidth * fading;
	const index flatWidth = realWidth - fadeWidth;

	IntMixer<> mixer;
	const auto border = colors.border;

	const double fadeDistanceStep = 1.0 / (realWidth * fading);
	double fadeDistance = 1.0;

	const auto lastStripIndex = minMaxBuffer.getLastStripIndex();

	index fadeBeginIndex = lastStripIndex + 1 + borderSize;
	if (fadeBeginIndex >= width) {
		fadeBeginIndex -= width;
	}

	index flatBeginIndex = fadeBeginIndex + fadeWidth;

	if (flatBeginIndex >= width) {
		for (index i = fadeBeginIndex; i < width; i++) {
			mixer.setParams(fadeDistance * fadeDistance);
			auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
			sc.a = mixer.mix(backgroundColor.a, sc.a);
			sc.r = mixer.mix(backgroundColor.r, sc.r);
			sc.g = mixer.mix(backgroundColor.g, sc.g);
			sc.b = mixer.mix(backgroundColor.b, sc.b);
			dest[i] = sc.full;

			fadeDistance -= fadeDistanceStep;
		}

		fadeBeginIndex = 0;
		flatBeginIndex -= width;
	}

	for (index i = fadeBeginIndex; i < flatBeginIndex; i++) {
		mixer.setParams(fadeDistance * fadeDistance);
		auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
		sc.a = mixer.mix(backgroundColor.a, sc.a);
		sc.r = mixer.mix(backgroundColor.r, sc.r);
		sc.g = mixer.mix(backgroundColor.g, sc.g);
		sc.b = mixer.mix(backgroundColor.b, sc.b);
		dest[i] = sc.full;

		fadeDistance -= fadeDistanceStep;
	}

	index borderBeginIndex = flatBeginIndex + flatWidth;
	if (borderBeginIndex >= width) {
		for (index i = flatBeginIndex; i < width; i++) {
			auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
			dest[i] = sc.full;
		}

		flatBeginIndex = 0;
		borderBeginIndex -= width;
	}

	for (index i = flatBeginIndex; i < borderBeginIndex; i++) {
		auto sc = isWaveAt(i, line) ? colors.wave : backgroundColor;
		dest[i] = sc.full;
	}

	index borderEndIndex = borderBeginIndex + borderSize;
	if (borderEndIndex >= width) {
		for (index i = borderBeginIndex; i < width; i++) {
			dest[i] = border.full;
		}
		borderBeginIndex = 0;
		borderEndIndex -= width;
	}
	for (index i = borderBeginIndex; i < borderEndIndex; i++) {
		dest[i] = border.full;
	}
}

bool WaveFormDrawer::isWaveAt(index i, index line) const {
	const auto minMax = minMaxBuffer.getPixels()[0][i];
	return line >= minMax.minPixel && line < minMax.maxPixel;
}
