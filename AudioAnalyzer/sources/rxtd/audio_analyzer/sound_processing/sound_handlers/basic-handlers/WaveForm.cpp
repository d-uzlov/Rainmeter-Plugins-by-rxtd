// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "WaveForm.h"
#include <filesystem>

#include "rxtd/std_fixes/MyMath.h"

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::WaveForm;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

using namespace std::string_literals;
using rxtd::audio_analyzer::image_utils::Color;

ParamsContainer WaveForm::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	params.width = context.parser.parseInt(context.options.get(L"width"), 100);
	if (params.width < 2) {
		context.log.error(L"width must be >= 2 but {} found", params.width);
		throw InvalidOptionsException{};
	}

	params.height = context.parser.parseInt(context.options.get(L"height"), 100);
	if (params.height < 2) {
		context.log.error(L"height must be >= 2 but {} found", params.height);
		throw InvalidOptionsException{};
	}

	if (params.width * params.height > 1000 * 1000) {
		context.log.warning(L"dangerously big width and height: {}, {}", params.width, params.height);
	}

	params.resolution = context.parser.parseFloat(context.options.get(L"resolution"), 50);
	if (params.resolution <= 0) {
		context.log.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.folder = context.rain.getPathFromCurrent(context.options.get(L"folder").asString() % own());

	params.colors.background = Color::parse(context.options.get(L"backgroundColor").asString(), context.parser, { 0, 0, 0 }).toIntColor();
	params.colors.wave = Color::parse(context.options.get(L"waveColor").asString(), context.parser, { 1, 1, 1 }).toIntColor();
	params.colors.line = Color::parse(context.options.get(L"lineColor").asString(), context.parser, { 0.5, 0.5, 0.5, 0.5 }).toIntColor();
	params.colors.border = Color::parse(context.options.get(L"borderColor").asString(), context.parser, { 1.0, 0.2, 0.2 }).toIntColor();

	if (const auto ldpString = context.options.get(L"lineDrawingPolicy").asIString(L"always");
		ldpString == L"always") {
		params.lineDrawingPolicy = LDP::eALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LDP::eBELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LDP::eNEVER;
	} else {
		context.log.warning(L"lineDrawingPolicy '{}' is not recognized", ldpString);
		throw InvalidOptionsException{};
	}

	params.stationary = context.parser.parseBool(context.options.get(L"stationary"), false);
	params.connected = context.parser.parseBool(context.options.get(L"connected"), true);

	params.borderSize = context.parser.parseInt(context.options.get(L"borderSize"), 0);
	params.borderSize = std::clamp<index>(params.borderSize, 0, params.width / 2);

	params.lineThickness = context.parser.parseInt(context.options.get(L"lineThickness"), 2 - (params.height & 1));
	params.lineThickness = std::clamp<index>(params.lineThickness, 0, params.height);

	params.fading = context.parser.parseFloat(context.options.get(L"FadingRatio"), 0.0);

	params.silenceThreshold = context.parser.parseFloatF(context.options.get(L"silenceThreshold"), -70);
	params.silenceThreshold = MyMath::db2amplitude(params.silenceThreshold);

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), context.parser, transformLogger);

	return params;
}

HandlerBase::ConfigurationResult
WaveForm::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	auto blockSize = static_cast<index>(static_cast<double>(sampleRate) * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minDistinguishableValue = 1.0 / static_cast<double>(params.height);

	drawer.setImageParams(params.width, params.height, params.stationary);
	drawer.setParams(
		params.connected, params.borderSize, params.fading,
		params.lineDrawingPolicy, params.lineThickness,
		params.colors
	);

	mainCounter.setBlockSize(blockSize);
	originalCounter.setBlockSize(blockSize);

	auto& snapshot = externalData.clear<Snapshot>();

	snapshotId++;
	updateSnapshot(snapshot);

	snapshot.folder = params.folder;

	snapshot.blockSize = blockSize;

	return { 0, {} };
}

void WaveForm::vProcess(ProcessContext context, ExternalData& externalData) {
	mainCounter.setWave(context.wave);
	originalCounter.setWave(context.originalWave);

	while (originalCounter.hasNext()) {
		mainCounter.update();
		originalCounter.update();

		snapshotId++;

		if (originalCounter.isBelowThreshold(params.silenceThreshold)) {
			drawer.fillSilence();
		} else {
			const auto transformedMin = std::abs(params.transformer.apply(std::abs(mainCounter.getMin())));
			const auto transformedMax = std::abs(params.transformer.apply(std::abs(mainCounter.getMax())));
			drawer.fillStrip(
				std::copysign(transformedMin, mainCounter.getMin()),
				std::copysign(transformedMax, mainCounter.getMax())
			);
		}

		mainCounter.skipBlock();
		originalCounter.skipBlock();
	}

	// consume the rest of the wave
	mainCounter.update();
	originalCounter.update();

	updateSnapshot(externalData.cast<Snapshot>());
}

void WaveForm::updateSnapshot(Snapshot& snapshot) {
	if (snapshot.id == snapshotId) {
		return;
	}
	snapshot.id = snapshotId;
	snapshot.writeNeeded = true;

	if (!(snapshot.empty && drawer.isEmpty())) {
		drawer.inflate();
		snapshot.pixels.copyWithResize(drawer.getResultBuffer());
		snapshot.empty = drawer.isEmpty();
	}
}

void WaveForm::staticFinisher(const Snapshot& snapshot, const ExternalMethods::CallContext& context) {
	auto& writeNeeded = snapshot.writeNeeded;
	if (!writeNeeded) {
		return;
	}

	context.buffer = snapshot.folder;
	context.buffer += context.filePrefix;
	context.buffer += L".bmp";

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, context.buffer);
	writeNeeded = false;
}

bool WaveForm::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternalMethods::CallContext& context
) {
	if (prop == L"file") {
		context.buffer = snapshot.folder;
		context.buffer += context.filePrefix;
		context.buffer += L".bmp";

		printer.print(context.buffer);
		return true;
	}
	if (prop == L"block size") {
		printer.print(snapshot.blockSize);
		return true;
	}

	return false;
}
