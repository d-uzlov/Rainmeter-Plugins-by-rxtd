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

using namespace std::string_literals;
using utils::Color;

using namespace audio_analyzer;

SoundHandler::ParseResult WaveForm::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	params.width = om.get(L"width").asInt(100);
	if (params.width < 2) {
		cl.error(L"width must be >= 2 but {} found", params.width);
		return { };
	}

	params.height = om.get(L"height").asInt(100);
	if (params.height < 2) {
		cl.error(L"height must be >= 2 but {} found", params.height);
		return { };
	}

	if (params.width * params.height > 1000 * 1000) {
		cl.warning(L"dangerously big width and height: {}, {}", params.width, params.height);
	}

	params.resolution = om.get(L"resolution").asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.folder = utils::FileWrapper::getAbsolutePath(
		om.get(L"folder").asString() % own(),
		rain.replaceVariables(L"[#CURRENTPATH]") % own()
	);

	params.colors.background = Color::parse(om.get(L"backgroundColor").asString(), { 0, 0, 0 }).toIntColor();
	params.colors.wave = Color::parse(om.get(L"waveColor").asString(), { 1, 1, 1 }).toIntColor();
	params.colors.line = Color::parse(om.get(L"lineColor").asString(), { 0.5, 0.5, 0.5, 0.5 }).toIntColor();
	params.colors.border = Color::parse(om.get(L"borderColor").asString(), { 1.0, 0.2, 0.2 }).toIntColor();

	if (const auto ldpString = om.get(L"lineDrawingPolicy").asIString(L"always");
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

	params.stationary = om.get(L"stationary").asBool(false);
	params.connected = om.get(L"connected").asBool(true);

	params.borderSize = om.get(L"borderSize").asInt(0);
	params.borderSize = std::clamp<index>(params.borderSize, 0, params.width / 2);

	params.fading = om.get(L"fadingPercent").asFloat(0.0);

	using TP = audio_utils::TransformationParser;
	if (legacyNumber < 104) {
		const auto gain = om.get(L"gain").asFloat(1.0);
		utils::BufferPrinter printer;
		printer.print(L"map[from 0 : 1, to 0 : {}]", gain);
		params.transformer = TP::parse(printer.getBufferView(), cl);
	} else {
		auto transformLogger = cl.context(L"transform: ");
		params.transformer = TP::parse(om.get(L"transform").asString(), transformLogger);
	}

	return { params, std::vector<istring>{ } };
}

SoundHandler::ConfigurationResult WaveForm::vConfigure(Logger& cl) {
	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	blockSize = index(sampleRate * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minDistinguishableValue = 1.0 / params.height;

	drawer.setImageParams(params.width, params.height, params.stationary);
	// drawer.setEdges(params.edges);
	drawer.setColors(params.colors);
	drawer.setLineDrawingPolicy(params.lineDrawingPolicy);
	drawer.setFading(params.fading);
	drawer.setConnected(params.connected);
	drawer.setBorderSize(params.borderSize);

	filepath = params.folder;
	filepath += L"wave-";
	filepath += config.channelName;
	filepath += L".bmp";

	minTransformer = { params.transformer };
	maxTransformer = { params.transformer };

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

void WaveForm::vProcess(array_view<float> wave, clock::time_point killTime) {
	for (const auto value : wave) {
		min = std::min<double>(min, value);
		max = std::max<double>(max, value);
		counter++;
		if (counter >= blockSize) {
			pushStrip(min, max);
			writeNeeded = true;

			counter = 0;
			min = 10.0;
			max = -10.0;
		}
	}

	const bool forced = !drawer.isEmpty();
	if (forced || !writerHelper.isEmptinessWritten()) {
		drawer.inflate();
	}
}

void WaveForm::vFinishStandalone() {
	if (!writeNeeded) {
		return;
	}

	writerHelper.write(drawer.getResultBuffer(), drawer.isEmpty(), filepath);
	writeNeeded = false;
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
}
