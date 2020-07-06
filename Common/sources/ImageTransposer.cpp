/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ImageTransposer.h"
#include "LinearInterpolator.h"

#include "undef.h"

void utils::ImageTransposer::setBackground(Color value) {
	backgroundColor = value;
}

void utils::ImageTransposer::transposeToBuffer(const Vector2D<Color>& imageData, index lineOffset, bool withFading, index gradientOffset) {
	const auto sourceWidth = imageData.getBufferSize();
	const auto sourceHeight = imageData.getBuffersCount();
	buffer.setBufferSize(sourceHeight);
	buffer.setBuffersCount(sourceWidth);

	LinearInterpolator interpolator { 0.0, double(sourceHeight), 0.0, 1.0 };

	for (index sourceY = lineOffset; sourceY < sourceHeight; sourceY++) {
		index gradientAmplification = sourceY - lineOffset - gradientOffset;
		if (gradientAmplification < 0) {
			gradientAmplification += sourceHeight;
		}
		float amplification = withFading ? interpolator.toValue(gradientAmplification) : 1.0;
		amplification = 1.0 - (1.0 - amplification) * (1.0 - amplification);
		transposeLine(sourceY - lineOffset, imageData[sourceY], amplification);
	}

	for (index sourceY = 0; sourceY < lineOffset; sourceY++) {
		index gradientAmplification = sourceY + sourceHeight - lineOffset - gradientOffset;
		if (gradientAmplification < 0) {
			gradientAmplification += sourceHeight;
		}
		float amplification = withFading ? interpolator.toValue(gradientAmplification) : 1.0;
		amplification = 1.0 - (1.0 - amplification) * (1.0 - amplification);
		transposeLine(sourceY + sourceHeight - lineOffset, imageData[sourceY], amplification);
	}
}

const utils::Vector2D<unsigned>& utils::ImageTransposer::getBuffer() const {
	return buffer;
}

void utils::ImageTransposer::transposeLine(index lineIndex, array_view<Color> lineData, float amplification) {
	for (index x = 0; x < lineData.size(); x++) {
		Color color = lineData[x] * amplification + backgroundColor * (1.0 - amplification);
		buffer[x][lineIndex] = color.toInt();
	}
}
