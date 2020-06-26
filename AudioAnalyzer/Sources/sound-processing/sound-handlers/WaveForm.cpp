/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveForm.h"
#include "BmpWriter.h"
#include "windows-wrappers/FileWrapper.h"
#include <filesystem>

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void WaveForm::setParams(const Params &_params) {
	if (params == _params) {
		return;
	}

	this->params = _params;

	if (_params.width <= 0 || _params.height <= 0) {
		return;
	}

	interpolator = { -1.0, 1.0, 0.0, _params.height - 1.0 };

	backgroundInt = _params.backgroundColor.toInt();
	waveInt = _params.waveColor.toInt();
	lineInt = _params.lineColor.toInt();

	imageBuffer.setBuffersCount(_params.width);
	imageBuffer.setBufferSize(_params.height);
	std::fill_n(imageBuffer[0].data(), _params.width * _params.height, backgroundInt);

	uint32_t color;
	if (_params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		color = lineInt;
	} else {
		color = waveInt;
	}
	const index centerPixel = std::lround(interpolator.toValue(0.0));
	for (index i = 0; i < _params.width; ++i) {
		imageBuffer[i][centerPixel] = color;
	}

	filepath.clear();

	utils::FileWrapper::createDirectories(_params.prefix);


	updateParams();
}

std::optional<WaveForm::Params> WaveForm::parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl, const utils::Rainmeter& rain) {
	Params params;

	params.width = optionMap.get(L"width"sv).asInt(100);
	if (params.width < 2) {
		cl.error(L"width must be >= 2 but {} found", params.width);
		return std::nullopt;
	}

	params.height = optionMap.get(L"height"sv).asInt(100);
	if (params.height < 2) {
		cl.error(L"height must be >= 2 but {} found", params.height);
		return std::nullopt;
	}
	params.minDistinguishableValue = 1.0 / params.height / 2.0; // below half pixel

	params.resolution = optionMap.get(L"resolution"sv).asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	string folder { optionMap.get(L"folder"sv).asString() };
	if (!folder.empty() && folder[0] == L'\"') {
		folder = folder.substr(1);
	}
	std::filesystem::path path { folder };
	if (!path.is_absolute()) {
		folder = rain.replaceVariables(L"[#CURRENTPATH]") %own() + folder;
	}
	folder = std::filesystem::absolute(folder).wstring();
	folder = LR"(\\?\)"s + folder;
	if (folder[folder.size() - 1] != L'\\') {
		folder += L'\\';
	}

	params.prefix = folder;

	params.backgroundColor = optionMap.get(L"backgroundColor"sv).asColor({ 0, 0, 0, 1 });

	params.waveColor = optionMap.get(L"waveColor"sv).asColor({ 1, 1, 1, 1 });
	params.lineColor = optionMap.get(L"lineColor"sv).asColor(params.waveColor);

	auto ldpString = optionMap.get(L"lineDrawingPolicy"sv).asIString();
	if (ldpString.empty() || ldpString == L"always") {
		params.lineDrawingPolicy = LineDrawingPolicy::ALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LineDrawingPolicy::BELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LineDrawingPolicy::NEVER;
	} else {
		cl.warning(L"lineDrawingPolicy '{}' not recognized, assume 'always'", ldpString);
		params.lineDrawingPolicy = LineDrawingPolicy::ALWAYS;
	}

	params.gain = optionMap.get(L"gain"sv).asFloat(1.0);

	return params;
}

void WaveForm::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;

	updateParams();
}

const wchar_t* WaveForm::getProp(const isview& prop) const {
	if (prop == L"file") {
		return filepath.c_str();
	} else if (prop == L"block size") {
		propString = std::to_wstring(blockSize);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void WaveForm::reset() {
	counter = 0;
	min = 10.0;
	max = -10.0;
}

void WaveForm::updateParams() {
	blockSize = index(samplesPerSec * params.resolution);
	reset();
}

void WaveForm::writeFile(const DataSupplier& dataSupplier) {
	auto writeBufferSize = params.width * params.height;
	auto writeBuffer = dataSupplier.getBuffer<uint32_t>(writeBufferSize);
	utils::BmpWriter::writeFile(filepath, imageBuffer[0].data(), params.height, params.width, lastIndex, writeBuffer.data(), writeBufferSize); // TODO remove .data()
}

void WaveForm::fillLine() {
	min *= params.gain;
	max *= params.gain;

	if (std::abs(min) < std::numeric_limits<float>::epsilon()) {
		min = 0.0;
	}
	if (std::abs(max) < std::numeric_limits<float>::epsilon()) {
		max = 0.0;
	}

	index minPixel = std::clamp<index>(std::lround(interpolator.toValue(min)), 0, params.height - 1);
	index maxPixel = std::clamp<index>(std::lround(interpolator.toValue(max)), 0, params.height - 1);
	if (minPixel == maxPixel) {
		if (maxPixel < params.height - 1) {
			maxPixel++;
		} else {
			minPixel--;
		}
	}

	auto buffer = imageBuffer[lastIndex];

	for (index i = 0; i < minPixel; ++i) {
		buffer[i] = backgroundInt;
	}
	for (index i = maxPixel; i < params.height; ++i) {
		buffer[i] = backgroundInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
		const index centerPixel = std::lround(interpolator.toValue(0.0));
		buffer[centerPixel] = lineInt;
	}

	for (index i = minPixel; i < maxPixel; ++i) {
		buffer[i] = waveInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		const index centerPixel = std::lround(interpolator.toValue(0.0));
		buffer[centerPixel] = lineInt;
	}

	lastIndex++;
	if (lastIndex >= params.width) {
		lastIndex = 0;
	}
}

void WaveForm::process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	if (filepath.empty()) {
		filepath = params.prefix;
		filepath += L"wave-";
		filepath += dataSupplier.getChannel().technicalName();
		filepath += L".bmp"sv;
	}

	const auto wave = dataSupplier.getWave();
	const auto waveSize = wave.size();

	const bool dataIsZero = std::all_of(wave.data(), wave.data() + waveSize, [=](auto x) { return x < params.minDistinguishableValue; });
	if (!dataIsZero) {
		lastNonZeroLine = 0;
	} else {
		lastNonZeroLine++;
	}
	if (lastNonZeroLine > params.width) {
		lastNonZeroLine = params.width;
		counter = 0;
		return;
	}

	for (index frame = 0; frame < waveSize; ++frame) {
		min = std::min<double>(min, wave[frame]);
		max = std::max<double>(max, wave[frame]);
		counter++;
		if (counter >= blockSize) {
			fillLine();
			counter = 0;
			min = 10.0;
			max = -10.0;
			changed = true;
		}
	}
}

void WaveForm::processSilence(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	if (filepath.empty()) {
		filepath = params.prefix;
		filepath += L"wave-";
		filepath += dataSupplier.getChannel().technicalName();
		filepath += L".bmp"sv;
	}

	const auto waveSize = dataSupplier.getWave().size();
	
	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const auto blockRemaining = blockSize - counter;
		min = std::min<double>(min, 0.0);
		max = std::max<double>(max, 0.0);
		changed = true;

		if (waveProcessed + blockRemaining <= waveSize) {
			fillLine();
			waveProcessed += blockRemaining;
			counter = 0;
			min = 10.0;
			max = -10.0;
		} else {
			counter = counter + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void WaveForm::finish(const DataSupplier& dataSupplier) {
	if (changed) {
		writeFile(dataSupplier);
		changed = false;
	}
}
