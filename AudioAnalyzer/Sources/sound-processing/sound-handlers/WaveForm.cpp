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
#include "option-parser/OptionMap.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool WaveForm::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr, index legacyNumber) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.width = optionMap.get(L"width"sv).asInt(100);
	if (params.width < 2) {
		cl.error(L"width must be >= 2 but {} found", params.width);
		return { };
	}

	params.height = optionMap.get(L"height"sv).asInt(100);
	if (params.height < 2) {
		cl.error(L"height must be >= 2 but {} found", params.height);
		return { };
	}

	if (params.width * params.height > 1000 * 1000) {
		cl.warning(L"dangerously big width and height: {}, {}", params.width, params.height);
	}

	params.resolution = optionMap.get(L"resolution"sv).asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.folder = utils::FileWrapper::getAbsolutePath(
		optionMap.get(L"folder"sv).asString() % own(),
		rain.replaceVariables(L"[#CURRENTPATH]") % own()
	);

	params.colors.background = optionMap.get(L"backgroundColor"sv).asColor({ 0, 0, 0, 1 }).toIntColor();
	params.colors.wave = optionMap.get(L"waveColor"sv).asColor({ 1, 1, 1, 1 }).toIntColor();
	params.colors.line = optionMap.get(L"lineColor"sv).asColor({ 0.5, 0.5, 0.5, 0.5 }).toIntColor();
	params.colors.border = optionMap.get(L"borderColor"sv).asColor({ 1.0, 0.2, 0.2, 1 }).toIntColor();

	if (const auto ldpString = optionMap.get(L"lineDrawingPolicy"sv).asIString(L"always");
		ldpString == L"always") {
		params.lineDrawingPolicy = LDP::eALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LDP::eBELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LDP::eNEVER;
	} else {
		cl.warning(L"lineDrawingPolicy '{}' is not recognized, assume 'always'", ldpString);
		params.lineDrawingPolicy = LDP::eALWAYS;
	}

	params.stationary = optionMap.get(L"stationary").asBool(false);
	params.connected = optionMap.get(L"connected").asBool(true);

	params.borderSize = optionMap.get(L"borderSize").asInt(0);
	params.borderSize = std::clamp<index>(params.borderSize, 0, params.width / 2);

	params.fading = optionMap.get(L"fadingPercent").asFloat(0.0);

	if (optionMap.has(L"transform"sv)) {
		params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), cl);
	} else if (optionMap.has(L"gain")) {
		// legacy
		const auto gain = optionMap.get(L"gain"sv).asFloat(1.0);
		utils::BufferPrinter printer;
		printer.print(L"map[from 0 ; 1][to 0 ; {}]", gain);
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ printer.getBufferView() }, cl);
	} else {
		params.transformer = { };
	}

	return true;
}

void WaveForm::setParams(const Params& value) {
	params = value;

	minDistinguishableValue = 1.0 / params.height / 2.0; // below half pixel

	drawer.setDimensions(params.width, params.height);
	// drawer.setEdges(params.edges);
	drawer.setColors(params.colors);
	drawer.setLineDrawingPolicy(params.lineDrawingPolicy);
	drawer.setStationary(params.stationary);
	drawer.setFading(params.fading);
	drawer.setConnected(params.connected);
	drawer.setBorderSize(params.borderSize);
}

SoundHandler::LinkingResult WaveForm::vFinishLinking(Logger& cl) {
	filepath = params.folder;
	filepath += L"wave-";
	filepath += getChannel().technicalName();
	filepath += L".bmp"sv;
	// todo check that can write ro this file

	minTransformer = { params.transformer };
	maxTransformer = { params.transformer };

	const index sampleRate = getSampleRate();
	blockSize = index(sampleRate * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minTransformer.updateTransformations(sampleRate, blockSize);
	maxTransformer.updateTransformations(sampleRate, blockSize);

	vReset();

	return { 0, 0 };
}

void WaveForm::vReset() {
	counter = 0;
	min = 10.0;
	max = -10.0;

	minTransformer.reset();
	maxTransformer.reset();
}

void WaveForm::vProcess(array_view<float> wave) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		// todo remove
		return;
	}

	for (const auto value : wave) {
		min = std::min<double>(min, value);
		max = std::max<double>(max, value);
		counter++;
		if (counter >= blockSize) {
			pushStrip(min, max);

			counter = 0;
			min = 10.0;
			max = -10.0;
		}
	}
}

void WaveForm::vFinish() {
	if (changed) {
		const bool forced = !drawer.isEmpty();
		if (forced || !writerHelper.isEmptinessWritten()) {
			drawer.inflate();
		}
		writerHelper.write(drawer.getResultBuffer(), drawer.isEmpty(), filepath);
		changed = false;
	}
}

bool WaveForm::vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
	if (prop == L"file") {
		printer.print(filepath);
		return true;
	}
	if (prop == L"block size") {
		printer.print(blockSize);
		return true;
	}

	return false;
}

void WaveForm::pushStrip(double min, double max) {
	min = minTransformer.apply(min);
	max = maxTransformer.apply(max);

	if (std::abs(min) < minDistinguishableValue && std::abs(max) < minDistinguishableValue) {
		drawer.fillSilence();
	} else {
		drawer.fillStrip(min, max);
	}

	changed = true;
}
