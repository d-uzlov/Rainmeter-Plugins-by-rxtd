/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "StripedImageFadeHelper.h"
#include "IntMixer.h"

#include "undef.h"

using namespace utils;

void StripedImageFadeHelper::inflate(array2d_view<IntColor> source) {
	const index height = source.getBuffersCount();

	resultBuffer.setBufferSize(source.getBufferSize());
	resultBuffer.setBuffersCount(source.getBuffersCount());

	for (int lineIndex = 0; lineIndex < height; ++lineIndex) {
		auto sourceLine = source[lineIndex];
		auto destLine = resultBuffer[lineIndex];

		inflateLine(sourceLine, destLine);
	}
}

void StripedImageFadeHelper::drawBorderInPlace(array2d_span<IntColor> source) const {
	const index height = source.getBuffersCount();

	for (int lineIndex = 0; lineIndex < height; ++lineIndex) {
		drawBorderInLine(source[lineIndex]);
	}
}

void StripedImageFadeHelper::inflateLine(array_view<IntColor> source, array_span<uint32_t> dest) const {
	const index width = source.size();

	const double realWidth = width - borderSize;

	const index fadeWidth = realWidth * fading;
	const index flatWidth = realWidth - fadeWidth;

	IntMixer<> mixer;
	const auto back = background;

	const double fadeDistanceStep = 1.0 / (realWidth * fading);
	double fadeDistance = 1.0;

	index fadeBeginIndex = pastLastStripIndex + borderSize;
	if (fadeBeginIndex >= width) {
		fadeBeginIndex -= width;
	}

	index flatBeginIndex = fadeBeginIndex + fadeWidth;

	if (flatBeginIndex >= width) {
		for (index i = fadeBeginIndex; i < width; i++) {
			mixer.setParams(fadeDistance * fadeDistance);
			auto sc = source[i];
			sc.a = mixer.mix(back.a, sc.a);
			sc.r = mixer.mix(back.r, sc.r);
			sc.g = mixer.mix(back.g, sc.g);
			sc.b = mixer.mix(back.b, sc.b);
			dest[i] = sc.full;

			fadeDistance -= fadeDistanceStep;
		}

		fadeBeginIndex = 0;
		flatBeginIndex -= width;
	}

	for (index i = fadeBeginIndex; i < flatBeginIndex; i++) {
		mixer.setParams(fadeDistance * fadeDistance);
		auto sc = source[i];
		sc.a = mixer.mix(back.a, sc.a);
		sc.r = mixer.mix(back.r, sc.r);
		sc.g = mixer.mix(back.g, sc.g);
		sc.b = mixer.mix(back.b, sc.b);
		dest[i] = sc.full;

		fadeDistance -= fadeDistanceStep;
	}

	index borderBeginIndex = flatBeginIndex + flatWidth;
	if (borderBeginIndex >= width) {
		for (index i = flatBeginIndex; i < width; i++) {
			dest[i] = source[i].full;
		}

		flatBeginIndex = 0;
		borderBeginIndex -= width;
	}

	for (index i = flatBeginIndex; i < borderBeginIndex; i++) {
		dest[i] = source[i].full;
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

void StripedImageFadeHelper::drawBorderInLine(array_span<IntColor> line) const {
	const index width = line.size();

	index borderBeginIndex = pastLastStripIndex;
	index borderEndIndex = borderBeginIndex + borderSize;
	if (borderEndIndex >= width) {
		for (index i = borderBeginIndex; i < width; i++) {
			line[i].full = border.full;
		}
		borderBeginIndex = 0;
		borderEndIndex -= width;
	}
	for (index i = borderBeginIndex; i < borderEndIndex; i++) {
		line[i].full = border.full;
	}
}
