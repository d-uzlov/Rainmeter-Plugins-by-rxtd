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
	auto& parser = context.parser;
	auto& options = context.options;

	params.width = parser.parse(options, L"width").valueOr(100);
	if (params.width < 2) {
		context.log.error(L"width: value must be >= 2 but {} found", params.width);
		throw InvalidOptionsException{};
	}

	params.height = parser.parse(options, L"height").valueOr(100);
	if (params.height < 2) {
		context.log.error(L"height: value must be >= 2 but {} found", params.height);
		throw InvalidOptionsException{};
	}

	if (params.width * params.height > 1000 * 1000) {
		context.log.warning(L"dangerously big width and height: {}, {}", params.width, params.height);
	}

	auto updateRate = context.parser.parse(context.options, L"UpdateRate").valueOr(20.0);
	if (updateRate < 1.0 || updateRate > 20000.0) {
		context.log.error(L"UpdateRate: invalid value {}, must be in range [1, 20000]", updateRate);
		throw InvalidOptionsException{};
	}
	params.resolution = 1.0 / updateRate;

	params.folder = context.rain.getPathFromCurrent(options.get(L"folder").asString() % own());

	Color::Mode defaultColorSpace;
	auto defaultColorSpaceOption = options.get(L"DefaultColorSpace").asIString(L"sRGB");
	if (auto defaultColorSpaceOpt = parseEnum<Color::Mode>(defaultColorSpaceOption);
		defaultColorSpaceOpt.has_value()) {
		defaultColorSpace = defaultColorSpaceOpt.value();
	} else {
		context.log.error(L"DefaultColorSpace: unknown value: {}", defaultColorSpaceOption);
		throw InvalidOptionsException{};
	}

	params.colors.background = parser.parse(options, L"backgroundColor").asCustomOr(Color{ 0.0f, 0.0f, 0.0f }, defaultColorSpace).toIntColor();
	params.colors.wave = parser.parse(options, L"waveColor").asCustomOr(Color{ 1.0f, 1.0f, 1.0f }, defaultColorSpace).toIntColor();
	params.colors.line = parser.parse(options, L"lineColor").asCustomOr(Color{ 0.5f, 0.5f, 0.5f, 0.5f }, defaultColorSpace).toIntColor();
	params.colors.border = parser.parse(options, L"borderColor").asCustomOr(Color{ 1.0f, 0.2f, 0.2f }, defaultColorSpace).toIntColor();

	if (const auto ldpString = options.get(L"lineDrawingPolicy").asIString(L"always");
		ldpString == L"always") {
		params.lineDrawingPolicy = LDP::eALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LDP::eBELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LDP::eNEVER;
	} else {
		context.log.warning(L"lineDrawingPolicy: unknown value: {}", ldpString);
		throw InvalidOptionsException{};
	}

	params.stationary = parser.parse(options, L"stationary").valueOr(false);
	params.connected = parser.parse(options, L"connected").valueOr(true);

	params.borderSize = parser.parse(options, L"borderSize").valueOr(0);
	params.borderSize = std::clamp<index>(params.borderSize, 0, params.width / 2);

	params.lineThickness = parser.parse(options, L"lineThickness").valueOr(2 - (params.height & 1));
	params.lineThickness = std::clamp<index>(params.lineThickness, 0, params.height);

	params.fading = parser.parse(options, L"FadingRatio").valueOr(0.0);

	params.silenceThreshold = parser.parse(options, L"silenceThreshold").valueOr(-70.0f);
	params.silenceThreshold = MyMath::db2amplitude(params.silenceThreshold);

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(options.get(L"transform").asString(), parser, transformLogger);

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

	context.printer.print(L"{}{}.bmp", snapshot.folder, context.filePrefix);

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, context.printer.getBufferView());
	writeNeeded = false;
}

bool WaveForm::getProp(
	const Snapshot& snapshot,
	isview prop,
	const ExternalMethods::CallContext& context
) {
	if (prop == L"file") {
		context.printer.print(L"{}{}.bmp", snapshot.folder, context.filePrefix);
		return true;
	}

	return false;
}
