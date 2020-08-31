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

	ParseResult result{ true };
	result.params = std::move(params);
	result.externalMethods.finish = wrapExternalMethod<Snapshot, &staticFinisher>();
	result.externalMethods.getProp = wrapExternalMethod<Snapshot, &getProp>();
	return result;
}

SoundHandler::ConfigurationResult WaveForm::vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	blockSize = index(sampleRate * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minDistinguishableValue = 1.0 / params.height;

	drawer.setImageParams(params.width, params.height, params.stationary);
	drawer.setColors(params.colors);
	drawer.setLineDrawingPolicy(params.lineDrawingPolicy);
	drawer.setFading(params.fading);
	drawer.setConnected(params.connected);
	drawer.setBorderSize(params.borderSize);

	drawer.inflate();

	minTransformer = { params.transformer };
	maxTransformer = { params.transformer };

	minTransformer.updateTransformations(sampleRate, blockSize);
	maxTransformer.updateTransformations(sampleRate, blockSize);

	writeNeeded = true;

	counter = 0;
	min = 10.0;
	max = -10.0;

	minTransformer.reset();
	maxTransformer.reset();


	if (nullptr == std::any_cast<Snapshot>(&snapshotAny)) {
		snapshotAny = Snapshot{ };
	}
	auto& snapshot = *std::any_cast<Snapshot>(&snapshotAny);

	snapshot.prefix = params.folder;
	snapshot.prefix += L"wave-";

	snapshot.blockSize = blockSize;

	snapshot.pixels.setBuffersCount(params.height);
	snapshot.pixels.setBufferSize(params.width);

	snapshot.pixels.copyWithResize(drawer.getResultBuffer());

	snapshot.writeNeeded = true;
	snapshot.empty = false;


	return { 0, 0 };
}

void WaveForm::vProcess(array_view<float> wave, clock::time_point killTime) {
	const bool wasEmpty = drawer.isEmpty();

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

	if (!drawer.isEmpty() || wasEmpty != drawer.isEmpty()) {
		drawer.inflate();
	}
}

void WaveForm::vUpdateSnapshot(std::any& handlerSpecificData) const {
	if (!writeNeeded) {
		return;
	}

	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);
	snapshot.writeNeeded = true;
	snapshot.empty = drawer.isEmpty();

	snapshot.pixels.copyWithResize(drawer.getResultBuffer());

	writeNeeded = false;
}

void WaveForm::staticFinisher(const Snapshot& snapshot, const ExternCallContext& context) {
	auto& writeNeeded = snapshot.writeNeeded;
	if (!writeNeeded) {
		return;
	}

	snapshot.filenameBuffer = snapshot.prefix;
	snapshot.filenameBuffer += context.channelName;
	snapshot.filenameBuffer += L".bmp";

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, snapshot.filenameBuffer);
	writeNeeded = false;
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

bool WaveForm::getProp(
	const Snapshot& snapshot,
	isview prop,
	utils::BufferPrinter& printer,
	const ExternCallContext& context
) {
	if (prop == L"file") {
		snapshot.filenameBuffer = snapshot.prefix;
		snapshot.filenameBuffer += context.channelName;
		snapshot.filenameBuffer += L".bmp";

		printer.print(snapshot.filenameBuffer);
		return true;
	}
	if (prop == L"block size") {
		printer.print(snapshot.blockSize);
		return true;
	}

	return false;
}
