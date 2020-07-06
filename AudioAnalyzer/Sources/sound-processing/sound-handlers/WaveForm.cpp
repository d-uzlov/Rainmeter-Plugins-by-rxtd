/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveForm.h"
#include <filesystem>
#include "windows-wrappers/FileWrapper.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void WaveForm::setParams(const Params &_params, Channel channel) {
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

	image.setBackground(backgroundInt);
	// yes, width to height, and vice versa — there is no error
	// image will be transposed later
	image.setImageWidth(_params.height);
	image.setImageHeight(_params.width);

	uint32_t color;
	if (_params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		color = lineInt;
	} else {
		color = waveInt;
	}
	const index centerPixel = interpolator.toValueDiscrete(0.0);
	for (index i = 0; i < _params.width; ++i) {
		auto line = image.fillNextLineManual();
		line[centerPixel] = color;
	}

	filepath = params.prefix;
	filepath += L"wave-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

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

void WaveForm::fillLine(array_span<uint32_t> buffer) {
	min *= params.gain;
	max *= params.gain;

	if (std::abs(min) < std::numeric_limits<float>::epsilon()) {
		min = 0.0;
	}
	if (std::abs(max) < std::numeric_limits<float>::epsilon()) {
		max = 0.0;
	}

	index minPixel = interpolator.toValueDiscrete(min);
	index maxPixel = interpolator.toValueDiscrete(max);
	if (minPixel == maxPixel) {
		if (maxPixel < params.height - 1) {
			maxPixel++;
		} else {
			minPixel--;
		}
	}

	for (index i = 0; i < minPixel; ++i) {
		buffer[i] = backgroundInt;
	}
	for (index i = maxPixel; i < params.height; ++i) {
		buffer[i] = backgroundInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
		const index centerPixel = interpolator.toValueDiscrete(0.0);
		buffer[centerPixel] = lineInt;
	}

	for (index i = minPixel; i < maxPixel; ++i) {
		buffer[i] = waveInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		const index centerPixel = interpolator.toValueDiscrete(0.0);
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

	const auto wave = dataSupplier.getWave();

	const bool dataIsZero = std::all_of(wave.begin(), wave.end(), [=](auto x) { return std::abs(x) < params.minDistinguishableValue; });

	for (const auto value : wave) {
		min = std::min<double>(min, value);
		max = std::max<double>(max, value);
		counter++;
		if (counter >= blockSize) {
			fillLine(dataIsZero ? image.fillNextLineManual() : image.nextLine());
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

	const auto waveSize = dataSupplier.getWave().size();
	
	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const auto blockRemaining = blockSize - counter;
		min = std::min<double>(min, 0.0);
		max = std::max<double>(max, 0.0);
		changed = true;

		if (waveProcessed + blockRemaining <= waveSize) {
			fillLine(image.fillNextLineManual());
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
		image.writeTransposed(filepath);
		changed = false;
	}
}
