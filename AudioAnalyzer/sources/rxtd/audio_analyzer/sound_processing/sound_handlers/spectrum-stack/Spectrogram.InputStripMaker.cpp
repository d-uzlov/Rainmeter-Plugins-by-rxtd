/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Spectrogram.h"

#include <filesystem>

#include "rxtd/LinearInterpolator.h"
#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/std_fixes/MyMath.h"

using namespace std::string_literals;

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::Spectrogram;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

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
