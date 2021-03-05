// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "Spectrogram.h"

#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/std_fixes/MyMath.h"

using namespace std::string_literals;

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::Spectrogram;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer Spectrogram::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	const auto sourceId = context.options.get(L"source").asIString();
	if (sourceId.empty()) {
		context.log.error(L"source is not found");
		throw InvalidOptionsException{};
	}

	params.length = context.parser.parseInt(context.options.get(L"length"), 100);
	if (params.length < 2) {
		context.log.error(L"length must be >= 2 but {} found", params.length);
		throw InvalidOptionsException{};
	}
	if (params.length >= 1500) {
		context.log.warning(L"dangerously large length {}", params.length);
	}

	params.resolution = context.parser.parseFloat(context.options.get(L"resolution"), 50);
	if (params.resolution <= 0) {
		context.log.error(L"resolution must be > 0 but {} found", params.resolution);
		throw InvalidOptionsException{};
	}
	params.resolution *= 0.001;

	params.folder = context.rain.getPathFromCurrent(context.options.get(L"folder").asString() % own());

	using MixMode = Color::Mode;
	if (auto mixMode = context.options.get(L"mixMode").asIString(L"srgb");
		mixMode == L"sRGB") {
		params.mixMode = MixMode::eRGB;
	} else if (mixMode == L"hsv") {
		params.mixMode = MixMode::eHSV;
	} else if (mixMode == L"hsl") {
		params.mixMode = MixMode::eHSL;
	} else if (mixMode == L"yCbCr") {
		params.mixMode = MixMode::eRGB; // difference between yCbCr and sRGB is linear
	} else {
		context.log.error(L"unknown mixMode '{}'", mixMode);
		throw InvalidOptionsException{};
	}

	if (!context.options.get(L"colors").empty()) {
		auto colorsDescriptionList = context.options.get(L"colors").asList(L';');
		parseColors(params.colors, params.colorLevels, colorsDescriptionList, context.parser, context.log.context(L"colors: "));
	} else {
		params.colors.resize(2);
		params.colors[0].color = Color::parse(context.options.get(L"baseColor").asString(), context.parser, { 0.0f, 0.0f, 0.0f });
		params.colors[1].color = Color::parse(context.options.get(L"maxColor").asString(), context.parser, { 1.0f, 1.0f, 1.0f });
		params.colorLevels.push_back(0.0f);
		params.colorLevels.push_back(1.0f);
	}

	for (auto& [wi, color] : params.colors) {
		color = color.convert(params.mixMode);
	}

	params.borderColor = Color::parse(context.options.get(L"borderColor").asString(), context.parser, { 1.0f, 0.2f, 0.2f });

	params.fading = std::clamp(context.parser.parseFloat(context.options.get(L"FadingRatio"), 0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(context.parser.parseInt(context.options.get(L"borderSize"), 0), 0, params.length / 2);

	params.stationary = context.parser.parseBool(context.options.get(L"stationary"), false);

	params.silenceThreshold = context.parser.parseFloatF(context.options.get(L"silenceThreshold"), -70);
	params.silenceThreshold = MyMath::db2amplitude(params.silenceThreshold);

	return params;
}

HandlerBase::ConfigurationResult
Spectrogram::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	index blockSize = static_cast<index>(static_cast<double>(sampleRate) * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	const index width = params.length;
	const index height = getConfiguration().sourcePtr->getDataSize().valuesCount;

	const auto backgroundIntColor = params.colors[0].color.toIntColor();
	image.setParams(width, height, backgroundIntColor, params.stationary);

	fadeHelper.setParams(backgroundIntColor, params.borderSize, params.borderColor.toIntColor(), params.fading);

	inputStripMaker.setParams(
		blockSize,
		config.sourcePtr->getDataSize().eqWaveSizes[0],
		height,
		params.colors, params.colorLevels
	);

	minMaxCounter.setBlockSize(blockSize);

	snapshotId++;

	auto& snapshot = externalData.clear<Snapshot>();
	updateSnapshot(snapshot);

	snapshot.folder = params.folder;

	snapshot.blockSize = blockSize;

	return { 0, {} };
}

void Spectrogram::parseColors(std::vector<ColorDescription>& resultColors, std::vector<float>& levels, OptionList list, option_parsing::OptionParser& parser, Logger& cl) {
	float prevValue = -std::numeric_limits<float>::infinity();

	bool first = true;
	for (auto colorsDescription : list) {
		auto [valueOpt, colorOpt] = colorsDescription.breakFirst(L':');

		if (colorOpt.empty()) {
			cl.error(L"description '{}' doesn't contain color", colorsDescription.asString());
			throw InvalidOptionsException{};
		}

		float value = parser.parseFloatF(valueOpt);

		if (value <= prevValue) {
			cl.error(L"values {} and {}: values must be increasing", prevValue, value);
			throw InvalidOptionsException{};
		}
		if (value / prevValue < 1.001f && value - prevValue < 0.001f) {
			// todo add proper comparison method
			cl.error(L"values {} and {} are too close", prevValue, value);
			throw InvalidOptionsException{};
		}

		levels.push_back(value);
		const auto color = Color::parse(colorOpt.asString(), parser);
		if (first) {
			first = false;
		} else {
			resultColors.back().widthInverted = 1.0f / (value - prevValue);
		}
		resultColors.push_back(ColorDescription{ 0.0f, color });

		prevValue = value;
	}

	if (resultColors.size() < 2) {
		cl.error(L"need at least 2 colors but {} found", resultColors.size());
		throw InvalidOptionsException{};
	}
}

void Spectrogram::vProcess(ProcessContext context, ExternalData& externalData) {
	minMaxCounter.setWave(context.originalWave);

	auto& source = *getConfiguration().sourcePtr;

	inputStripMaker.setChunks(source.getChunks(0));

	while (minMaxCounter.hasNext()) {
		minMaxCounter.update();

		inputStripMaker.next();
		snapshotId++;

		if (minMaxCounter.isBelowThreshold(params.silenceThreshold)) {
			image.pushEmptyStrip(params.colors[0].color.toIntColor());
		} else {
			image.pushStrip(inputStripMaker.getBuffer());
		}

		minMaxCounter.skipBlock();
	}

	// consume the rest of the wave
	minMaxCounter.update();

	updateSnapshot(externalData.cast<Snapshot>());
}

void Spectrogram::updateSnapshot(Snapshot& snapshot) {
	if (snapshot.id == snapshotId) {
		return;
	}
	snapshot.id = snapshotId;
	snapshot.writeNeeded = true;

	if (!(snapshot.empty && image.isEmpty())) {
		if (params.fading != 0.0) {
			fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
			fadeHelper.inflate(image.getPixels());
		} else {
			if (params.borderSize != 0) {
				fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
				fadeHelper.drawBorderInPlace(image.getPixelsWritable());
			}
		}

		snapshot.pixels.copyWithResize(params.fading != 0.0 ? fadeHelper.getResultBuffer() : image.getPixels());
		snapshot.empty = image.isEmpty();
	}
}

void Spectrogram::staticFinisher(const Snapshot& snapshot, const ExternalMethods::CallContext& context) {
	if (!snapshot.writeNeeded) {
		return;
	}

	context.buffer = snapshot.folder;
	context.buffer += context.filePrefix;
	context.buffer += L".bmp";

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, context.buffer);
	snapshot.writeNeeded = false;
}

bool Spectrogram::getProp(
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
