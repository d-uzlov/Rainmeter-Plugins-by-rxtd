/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveForm.h"
#include <string_view>
#include <algorithm>
#include "BandAnalyzer.h"
#include "BmpWriter.h"
#include "windows-wrappers/FileWrapper.h"
#include <filesystem>

#pragma warning(disable : 4458)
#pragma warning(disable : 4244)

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

void rxaa::WaveForm::setParams(const Params &_params) {
	this->params = _params;

	if (params.width <= 0 || params.height <= 0) {
		return;
	}

	interpolator = { -1.0, 1.0, 0.0, params.height - 1.0 };

	backgroundInt = params.backgroundColor.toInt();
	waveInt = params.waveColor.toInt();
	lineInt = params.lineColor.toInt();

	imageBuffer.setBuffersCount(params.width);
	imageBuffer.setBufferSize(params.height);
	std::fill_n(imageBuffer[0], params.width* params.height, backgroundInt);

	uint32_t color;
	if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		color = lineInt;
	} else {
		color = waveInt;
	}
	const unsigned centerPixel = std::lround(interpolator.toValue(0.0));
	for (unsigned i = 0; i < this->params.width; ++i) {
		imageBuffer[i][centerPixel] = color;
	}

	filepath.clear();

	rxu::FileWrapper::createDirectories(params.prefix);

	updateParams();
}

std::optional<rxaa::WaveForm::Params> rxaa::WaveForm::parseParams(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger& cl, const rxu::Rainmeter& rain) {
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

	params.resolution = optionMap.get(L"resolution"sv).asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	std::wstring folder { optionMap.getCS(L"folder"sv).asString() };
	if (!folder.empty() && folder[0] == L'\"') {
		folder = folder.substr(1);
	}
	std::filesystem::path path { folder };
	if (!path.is_absolute()) {
		folder = rain.replaceVariables(L"[#CURRENTPATH]") + folder;
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

	auto ldpString = optionMap.getCS(L"lineDrawingPolicy"sv).asString();
	if (ldpString.empty() || ldpString == L"always"sv) {
		params.lineDrawingPolicy = LineDrawingPolicy::ALWAYS;
	} else if (ldpString == L"belowWave"sv) {
		params.lineDrawingPolicy = LineDrawingPolicy::BELOW_WAVE;
	} else if (ldpString == L"never"sv) {
		params.lineDrawingPolicy = LineDrawingPolicy::NEVER;
	} else {
		cl.warning(L"lineDrawingPolicy '{}' now recognized, assume 'always'", ldpString);
		params.lineDrawingPolicy = LineDrawingPolicy::ALWAYS;
	}

	params.gain = optionMap.get(L"gain"sv).asFloat(1.0);

	return params;
}

const double* rxaa::WaveForm::getData() const {
	return &result;
}

size_t rxaa::WaveForm::getCount() const {
	return 0;
}

void rxaa::WaveForm::setSamplesPerSec(uint32_t samplesPerSec) {
	this->samplesPerSec = samplesPerSec;

	updateParams();
}

const wchar_t* rxaa::WaveForm::getProp(const std::wstring_view& prop) {
	if (prop == L"file"sv) {
		return filepath.c_str();
	} else if (prop == L"block size"sv) {
		propString = std::to_wstring(blockSize);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void rxaa::WaveForm::reset() {
	counter = 0;
	min = 10.0;
	max = -10.0;
}

void rxaa::WaveForm::updateParams() {
	blockSize = samplesPerSec * params.resolution;
	reset();
}

void rxaa::WaveForm::writeFile(const DataSupplier& dataSupplier) {
	auto writeBufferSize = params.width * params.height;
	auto writeBuffer = dataSupplier.getBuffer<uint32_t>(writeBufferSize);
	rxu::BmpWriter::writeFile(filepath, imageBuffer[0], params.height, params.width, lastIndex, writeBuffer, writeBufferSize);
}

void rxaa::WaveForm::fillLine() {
	min *= params.gain;
	max *= params.gain;

	if (std::abs(min) < std::numeric_limits<float>::epsilon()) {
		min = 0.0;
	}
	if (std::abs(max) < std::numeric_limits<float>::epsilon()) {
		max = 0.0;
	}

	unsigned minPixel = std::clamp<int>(std::lround(interpolator.toValue(min)), 0, params.height - 1);
	unsigned maxPixel = std::clamp<int>(std::lround(interpolator.toValue(max)), 0, params.height - 1);
	if (minPixel == maxPixel) {
		if (maxPixel < params.height - 1) {
			maxPixel++;
		} else {
			minPixel--;
		}
	}

	const auto buffer = imageBuffer[lastIndex];

	for (unsigned i = 0; i < minPixel; ++i) {
		buffer[i] = backgroundInt;
	}
	for (unsigned i = maxPixel; i < params.height; ++i) {
		buffer[i] = backgroundInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
		const unsigned centerPixel = std::lround(interpolator.toValue(0.0));
		buffer[centerPixel] = lineInt;
	}

	for (unsigned i = minPixel; i < maxPixel; ++i) {
		buffer[i] = waveInt;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		const unsigned centerPixel = std::lround(interpolator.toValue(0.0));
		buffer[centerPixel] = lineInt;
	}

	lastIndex++;
	if (lastIndex >= params.width) {
		lastIndex = 0;
	}
}

void rxaa::WaveForm::process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	if (filepath.empty()) {
		filepath = params.prefix;
		filepath += L"wave-";
		filepath += dataSupplier.getChannel().toString();
		filepath += L".bmp"sv;
	}

	const auto wave = dataSupplier.getWave();
	const auto waveSize = dataSupplier.getWaveSize();

	bool changed = false;

	for (unsigned int frame = 0; frame < waveSize; ++frame) {
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

	if (changed) {
		writeFile(dataSupplier);
	}
}

void rxaa::WaveForm::processSilence(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	if (filepath.empty()) {
		filepath = params.prefix;
		filepath += L"wave-";
		filepath += dataSupplier.getChannel().toString();
		filepath += L".bmp"sv;
	}

	const auto waveSize = dataSupplier.getWaveSize();

	bool changed = false;

	// TODO rewrite
	for (unsigned int frame = 0; frame < waveSize; ++frame) {
		min = std::min<double>(min, 0.0);
		max = std::max<double>(max, 0.0);
		counter++;
		if (counter >= blockSize) {
			fillLine();
			counter = 0;
			min = 10.0;
			max = -10.0;
			changed = true;
		}
	}

	if (changed) {
		writeFile(dataSupplier);
	}
}
