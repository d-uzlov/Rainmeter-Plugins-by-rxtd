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
		folder = rain.replaceVariables(L"[#CURRENTPATH]") % own() + folder;
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
		params.lineDrawingPolicy = LDP::ALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LDP::BELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LDP::NEVER;
	} else {
		cl.warning(L"lineDrawingPolicy '{}' not recognized, assume 'always'", ldpString);
		params.lineDrawingPolicy = LDP::ALWAYS;
	}

	params.gain = optionMap.get(L"gain"sv).asFloat(1.0);

	params.peakAntialiasing = optionMap.get(L"peakAntialiasing").asBool(false);
	params.supersamplingSize = optionMap.get(L"supersamplingSize").asInt(1);
	if (params.supersamplingSize < 1) {
		cl.error(L"supersamplingSize must be > 1 but {} found. Assume 1", params.supersamplingSize);
		params.supersamplingSize = 1;
	}

	params.moving = optionMap.get(L"moving").asBool(true);
	params.fading = optionMap.get(L"fading").asBool(false);

	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), cl);

	return params;
}

double WaveForm::WaveformValueTransformer::apply(double value) {
	const bool positive = value > 0;
	value = cvt.apply(std::abs(value));

	if (!positive) {
		value *= -1;
	}

	return value;
}

void WaveForm::setParams(const Params &_params, Channel channel) {
	if (params == _params) {
		return;
	}

	this->params = _params;

	if (_params.width <= 0 || _params.height <= 0) {
		return;
	}

	minDistinguishableValue = 1.0 / params.height / 2.0; // below half pixel

	drawer.setDimensions(params.width, params.height);
	drawer.setEdgeAntialiasing(params.peakAntialiasing);
	drawer.setColors(params.backgroundColor, params.waveColor, params.lineColor);
	drawer.setLineDrawingPolicy(params.lineDrawingPolicy);

	filepath = params.prefix;
	filepath += L"wave-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

	minTransformer = { params.transformer };
	maxTransformer = { params.transformer };

	utils::FileWrapper::createDirectories(_params.prefix);

	updateParams();
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
	blockSize = index(samplesPerSec * params.resolution / params.supersamplingSize);
	blockSize = std::max<index>(blockSize, 1);
	reset();
}

void WaveForm::pushStrip(double min, double max) {
	min = minTransformer.apply(min);
	max = maxTransformer.apply(max);

	if (std::abs(min) < minDistinguishableValue && std::abs(max) < minDistinguishableValue) {
		drawer.fillSilence();
	}

	drawer.fillStrip(min, max);
}

void WaveForm::process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	const auto wave = dataSupplier.getWave();

	for (const auto value : wave) {
		min = std::min<double>(min, value);
		max = std::max<double>(max, value);
		counter++;
		if (counter >= blockSize) {
			pushStrip(min, max);

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
			pushStrip(min, max);

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
		drawer.inflate();
		writerHelper.write(drawer.getResultBuffer(), drawer.isEmpty(), filepath);
		// image.writeTransposed(filepath, params.moving, params.fading);
		changed = false;
	}
}
