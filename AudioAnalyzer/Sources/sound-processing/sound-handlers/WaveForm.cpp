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

	params.peakAntialiasing = optionMap.get(L"peakAntialiasing").asBool(false);
	params.supersamplingSize = optionMap.get(L"supersamplingSize").asInt(1);
	if (params.supersamplingSize < 1) {
		cl.error(L"supersamplingSize must be > 1 but {} found. Assume 1", params.supersamplingSize);
		params.supersamplingSize = 1;
	}

	params.moving = optionMap.get(L"moving").asBool(true);
	params.fading = optionMap.get(L"fading").asBool(false);

	return params;
}

void WaveForm::setParams(const Params &_params, Channel channel) {
	if (params == _params) {
		return;
	}

	this->params = _params;

	if (_params.width <= 0 || _params.height <= 0) {
		return;
	}

	interpolator = { -1.0, 1.0, 0, _params.height - 1 };

	image.setBackground(params.backgroundColor.toInt());
	image.setWidth(_params.width);
	image.setHeight(_params.height);
	
	uint32_t color;
	if (_params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		color = params.lineColor.toInt();
	} else {
		color = params.waveColor.toInt();
	}

	std::vector<uint32_t> defaultStrip;
	defaultStrip.resize(_params.height);
	std::fill(defaultStrip.begin(), defaultStrip.end(), color);

	const index centerPixel = interpolator.toValueD(0.0);
	defaultStrip[centerPixel] = color;

	for (index i = 0; i < _params.width * params.supersamplingSize; ++i) {
		image.pushEmptyStrip(defaultStrip);
	}

	filepath = params.prefix;
	filepath += L"wave-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

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

void WaveForm::fillLine(array_span<utils::Color> buffer) {
	min *= params.gain;
	max *= params.gain;

	min = std::clamp(min, -1.0, 1.0);
	max = std::clamp(max, -1.0, 1.0);
	min = std::min(min, max);
	max = std::max(min, max);

	auto minPixel = interpolator.toValue(min);
	auto maxPixel = interpolator.toValue(max);
	const auto minMaxDelta = maxPixel - minPixel;
	if (minMaxDelta < 1) {
		const auto average = (minPixel + maxPixel) * 0.5;
		minPixel = average - 0.5;
		maxPixel = minPixel + 1.0; // max should always be >= min + 1

		const auto minD = interpolator.makeDiscreet(minPixel);
		const auto minDC = interpolator.makeDiscreetClamped(minPixel);
		if (minD != minDC) {
			const double shift = 1.0 - interpolator.percentRelativeToNext(minPixel);
			minPixel += shift;
			maxPixel += shift;
		}

		const auto maxD = interpolator.makeDiscreet(maxPixel);
		const auto maxDC = interpolator.makeDiscreetClamped(maxPixel);
		if (maxD != maxDC) {
			const double shift = -interpolator.percentRelativeToNext(maxPixel);
			minPixel += shift;
			maxPixel += shift;
		}
	}

	const auto lowBackgroundBound = interpolator.makeDiscreetClamped(minPixel);
	const auto highBackgroundBound = interpolator.makeDiscreetClamped(maxPixel);

	// [0, lowBackgroundBound) ← background
	// [lowBackgroundBound, lowBackgroundBound] ← transition
	// [lowBackgroundBound + 1, highBackgroundBound) ← line
	// [highBackgroundBound, highBackgroundBound] ← transition
	// [highBackgroundBound + 1, MAX] ← background

	// Should never really clamp due to minPixel <= maxPixel - 1
	// Because discreet(maxPixel) and discreet(minPixel) shouldn't clamp after shift above
	// If (max - min) was already >= 1.0, then also no clamp because discreet(toValue([-1.0, 1.0])) should be within clamp range
	const auto lowLineBound = interpolator.clamp(lowBackgroundBound + 1);
	const auto highLineBound = highBackgroundBound;

	for (index i = 0; i < lowBackgroundBound; ++i) {
		buffer[i] = params.backgroundColor;
	}
	for (index i = highBackgroundBound + 1; i < params.height; ++i) {
		buffer[i] = params.backgroundColor;
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
		const index centerPixel = interpolator.toValueD(0.0);
		buffer[centerPixel] = params.lineColor;
	}

	const double lowPercent = interpolator.percentRelativeToNext(minPixel);
	if (params.peakAntialiasing) {
		const auto lowTransitionColor = params.backgroundColor * lowPercent + params.waveColor * (1.0 - lowPercent);
		buffer[lowBackgroundBound] = lowTransitionColor;
	} else {
		if (lowPercent < 0.5) {
			buffer[lowBackgroundBound] = params.waveColor;
		} else {
			buffer[lowBackgroundBound] = params.backgroundColor;
		}
	}

	for (index i = lowLineBound; i < highLineBound; i++) {
		buffer[i] = params.waveColor;
	}

	const double highPercent = interpolator.percentRelativeToNext(maxPixel);

	if (params.peakAntialiasing) {
		const auto highTransitionColor = params.backgroundColor * (1.0 - highPercent) + params.waveColor * highPercent;
		buffer[highBackgroundBound] = highTransitionColor;
	} else {
		if (highPercent > 0.5) {
			buffer[highBackgroundBound] = params.waveColor;
		} else {
			buffer[highBackgroundBound] = params.backgroundColor;
		}
	}

	if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
		const index centerPixel = interpolator.toValueD(0.0);
		buffer[centerPixel] = params.lineColor;
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
			//dataIsZero ? image.fillNextLineManual() : image.nextLine()

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
			// fillLine(image.fillNextLineManual());
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
		// image.writeTransposed(filepath, params.moving, params.fading);
		changed = false;
	}
}
