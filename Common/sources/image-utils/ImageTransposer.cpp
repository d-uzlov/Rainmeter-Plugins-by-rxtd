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

void utils::ImageTransposer::transposeToBufferSimple(const Vector2D<uint32_t>& imageData, index lineOffset) {
	const auto sourceWidth = imageData.getBufferSize();
	const auto sourceHeight = imageData.getBuffersCount();
	buffer.setBufferSize(sourceHeight);
	buffer.setBuffersCount(sourceWidth);

	for (index sourceY = lineOffset; sourceY < sourceHeight; sourceY++) {
		transposeLineSimple(sourceY - lineOffset, imageData[sourceY]);
	}

	for (index sourceY = 0; sourceY < lineOffset; sourceY++) {
		transposeLineSimple(sourceY + sourceHeight - lineOffset, imageData[sourceY]);
	}
}

void utils::ImageTransposer::transposeToBuffer(const Vector2D<float>& imageData, index lineOffset, bool withFading, index gradientOffset, Color c1, Color c2) {
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
		transposeLine(sourceY - lineOffset, imageData[sourceY], amplification, c1, c2);
	}

	for (index sourceY = 0; sourceY < lineOffset; sourceY++) {
		index gradientAmplification = sourceY + sourceHeight - lineOffset - gradientOffset;
		if (gradientAmplification < 0) {
			gradientAmplification += sourceHeight;
		}
		float amplification = withFading ? interpolator.toValue(gradientAmplification) : 1.0;
		amplification = 1.0 - (1.0 - amplification) * (1.0 - amplification);
		transposeLine(sourceY + sourceHeight - lineOffset, imageData[sourceY], amplification, c1, c2);
	}
}

const utils::Vector2D<unsigned>& utils::ImageTransposer::getBuffer() const {
	return buffer;
}

void utils::ImageTransposer::transposeLineSimple(index lineIndex, array_view<uint32_t> lineData) {
	for (index x = 0; x < lineData.size(); x++) {
		buffer[x][lineIndex] = lineData[x];
	}
}

void utils::ImageTransposer::transposeLine(index lineIndex, array_view<float> lineData, float amplification, Color c1, Color c2) {
	for (index x = 0; x < lineData.size(); x++) {
		Color color = Color::mix(lineData[x], c1, c2); // TODO
		color = Color::mix(amplification, color, backgroundColor);
		buffer[x][lineIndex] = color.toInt();
	}
}
