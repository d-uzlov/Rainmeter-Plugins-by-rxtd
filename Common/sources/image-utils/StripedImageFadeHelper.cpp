/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "StripedImageFadeHelper.h"

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
