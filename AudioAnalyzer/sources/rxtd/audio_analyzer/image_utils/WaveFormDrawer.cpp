/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveFormDrawer.h"

#include "rxtd/IntMixer.h"

using rxtd::audio_analyzer::image_utils::WaveFormDrawer;

void WaveFormDrawer::setImageParams(index width, index height, bool stationary) {
	if (width == this->width && height == this->height) {
		return;
	}

	interpolator = { -1.0, 1.0, 0, height - 1 };
	const index centerLineIndex = interpolator.toValue(0.0);

	minMaxBuffer.setParams(width, 1, { centerLineIndex, centerLineIndex }, stationary);
	resultBuffer.setBufferSize(width);
	resultBuffer.setBuffersCount(height);

	this->width = width;
	this->height = height;

	prev.minPixel = centerLineIndex;
	prev.maxPixel = centerLineIndex;
}

void WaveFormDrawer::fillSilence() {
	const index centerLineIndex = interpolator.toValue(0.0);
	const MinMax mm{ centerLineIndex, centerLineIndex };
	minMaxBuffer.pushEmptyStrip(mm);
}

void WaveFormDrawer::fillStrip(double min, double max) {
	min = std::clamp(min, -1.0, 1.0);
	max = std::clamp(max, -1.0, 1.0);
	min = std::min(min, max);
	max = std::max(min, max);

	auto minPixel = interpolator.toValue(min);
	auto maxPixel = interpolator.toValue(max);

	if (connected) {
		minPixel = std::min(minPixel, prev.maxPixel);
		maxPixel = std::max(maxPixel, prev.minPixel);

		prev.minPixel = minPixel;
		prev.maxPixel = maxPixel;
	}

	MinMax mm{ minPixel, maxPixel };
	minMaxBuffer.pushStrip({ &mm, 1 });
}

void WaveFormDrawer::inflate() {
	const index centerLineIndex = interpolator.toValue(0.0);
	const index lowLineBound = centerLineIndex - (lineThickness - 1) / 2;
	const index highLineBound = centerLineIndex + (lineThickness) / 2;

	for (index lineIndex = 0; lineIndex < lowLineBound; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
	}

	for (index i = lowLineBound; i <= highLineBound; ++i) {
		switch (lineDrawingPolicy) {
		case LineDrawingPolicy::eNEVER: {
			inflateLine(i, resultBuffer[i], colors.background);
			break;
		}
		case LineDrawingPolicy::eBELOW_WAVE: {
			inflateLine(i, resultBuffer[i], colors.line);
			break;
		}
		case LineDrawingPolicy::eALWAYS: {
			std::fill_n(resultBuffer[i].data(), resultBuffer.getBufferSize(), colors.line);
			break;
		}
		}
	}

	for (index lineIndex = highLineBound + 1; lineIndex < height; ++lineIndex) {
		inflateLine(lineIndex, resultBuffer[lineIndex], colors.background);
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
