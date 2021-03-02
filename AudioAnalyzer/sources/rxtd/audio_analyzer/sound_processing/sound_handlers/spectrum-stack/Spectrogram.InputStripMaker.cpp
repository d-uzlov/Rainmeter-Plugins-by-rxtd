/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Spectrogram.h"
#include "rxtd/LinearInterpolator.h"

using rxtd::audio_analyzer::handler::Spectrogram;

void Spectrogram::InputStripMaker::setParams(
	index _blockSize, index _chunkEquivalentWaveSize, index bufferSize, array_view<ColorDescription> _colors, array_view<float> _colorLevels
) {
	blockSize = _blockSize;
	chunkEquivalentWaveSize = _chunkEquivalentWaveSize;
	colors = _colors;
	colorLevels = _colorLevels;
	counter = 0;

	buffer.resize(static_cast<size_t>(bufferSize));
	std::fill(buffer.begin(), buffer.end(), colors[0].color.toIntColor());
}

void Spectrogram::InputStripMaker::next() {
	array_view<float> chunk;
	while (counter < blockSize && !chunks.empty()) {
		counter += chunkEquivalentWaveSize;
		chunk = chunks.front();
		chunks.remove_prefix(1);
	}

	if (counter >= blockSize) {
		counter -= blockSize;
	}

	if (!chunk.empty()) {
		if (colors.size() == 2) {
			// only use 2 colors
			fillStrip(chunk, buffer);
		} else {
			// many colors, but slightly slower
			fillStripMulticolor(chunk, buffer);
		}
	}
}

void Spectrogram::InputStripMaker::fillStrip(array_view<float> data, array_span<IntColor> buffer) const {
	const LinearInterpolator<float> interpolator{ colorLevels.front(), colorLevels.back(), 0.0f, 1.0f };
	const auto lowColor = colors[0].color;
	const auto highColor = colors[1].color;

	for (index i = 0; i < static_cast<index>(buffer.size()); ++i) {
		auto value = interpolator.toValue(data[i]);
		value = std::clamp(value, 0.0f, 1.0f);

		buffer[i] = (highColor * value + lowColor * (1.0f - value)).toIntColor();
	}
}

void Spectrogram::InputStripMaker::fillStripMulticolor(
	array_view<float> data, array_span<IntColor> buffer
) const {
	for (index i = 0; i < buffer.size(); ++i) {
		const auto value = std::clamp(data[i], colorLevels.front(), colorLevels.back());

		index lowColorIndex = 0;
		for (index j = 1; j < colors.size(); j++) {
			const auto colorHighValue = colorLevels[j];
			if (value <= colorHighValue) {
				lowColorIndex = j - 1;
				break;
			}
		}

		const auto lowColorValue = colorLevels[lowColorIndex];
		const auto intervalCoef = colors[lowColorIndex].widthInverted;
		const auto lowColor = colors[lowColorIndex].color;
		const auto highColor = colors[lowColorIndex + 1].color;

		const float percentValue = (value - lowColorValue) * intervalCoef;

		buffer[i] = (highColor * percentValue + lowColor * (1.0f - percentValue)).toIntColor();
	}
}
