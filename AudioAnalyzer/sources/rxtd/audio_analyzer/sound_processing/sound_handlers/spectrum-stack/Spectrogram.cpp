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

	params.length = context.parser.parse(context.options, L"length").valueOr(100);
	if (params.length < 2) {
		context.log.error(L"length must be >= 2 but {} found", params.length);
		throw InvalidOptionsException{};
	}
	if (params.length >= 1500) {
		context.log.warning(L"dangerously large length {}", params.length);
	}

	auto updateRate = context.parser.parse(context.options, L"UpdateRate").valueOr(20.0);
	if (updateRate < 1.0 || updateRate > 20000.0) {
		context.log.error(L"UpdateRate: invalid value {}, must be in range [1, 20000]", updateRate);
		throw InvalidOptionsException{};
	}
	params.resolution = 1.0 / updateRate;

	params.folder = context.rain.getPathFromCurrent(context.options.get(L"folder").asString() % own());

	Color::Mode defaultColorSpace;
	auto defaultColorSpaceStr = context.options.get(L"DefaultColorSpace").asIString(L"sRGB");
	if (auto defaultColorSpaceOpt = parseEnum<Color::Mode>(defaultColorSpaceStr);
		defaultColorSpaceOpt.has_value()) {
		defaultColorSpace = defaultColorSpaceOpt.value();
	} else {
		context.log.error(L"DefaultColorSpace: unknown value: {}", defaultColorSpaceStr);
		throw InvalidOptionsException{};
	}

	auto mixModeStr = context.options.get(L"mixMode").asIString(defaultColorSpaceStr);
	if (auto mixModeOpt = parseEnum<Color::Mode>(mixModeStr);
		mixModeOpt.has_value()) {
		auto value = mixModeOpt.value();
		if (value == Color::Mode::eYCBCR || value == Color::Mode::eHEX || value == Color::Mode::eRGB255) {
			// difference between yCbCr and sRGB is linear
			value = Color::Mode::eRGB;
		}
		params.mixMode = value;
	} else {
		context.log.error(L"mixMode: unknown value: {}", mixModeStr);
		throw InvalidOptionsException{};
	}

	if (!context.options.get(L"colors").empty()) {
		auto colorsDescriptionList = context.options.get(L"colors").asList(L';');
		std::tie(params.colors, params.colorLevels) = parseColors(colorsDescriptionList, defaultColorSpace, context.parser, context.log.context(L"colors: "));

		if (!context.options.get(L"baseColor").empty()
			|| !context.options.get(L"maxColor").empty()) {
			context.log.warning(L"option 'colors' disables 'baseColor' and 'maxColor' options");
		}
	} else {
		params.colors.resize(2);
		params.colors[0].color = context.parser.parse(context.options, L"baseColor").asCustomOr(Color{ 0.0f, 0.0f, 0.0f }, defaultColorSpace);
		params.colors[1].color = context.parser.parse(context.options, L"maxColor").asCustomOr(Color{ 1.0f, 1.0f, 1.0f }, defaultColorSpace);
		params.colorLevels.push_back(0.0f);
		params.colorLevels.push_back(1.0f);
	}

	for (auto& [wi, color] : params.colors) {
		color = color.convert(params.mixMode);
	}

	params.borderColor = context.parser.parse(context.options, L"borderColor").asCustomOr(Color{ 1.0f, 0.2f, 0.2f }, defaultColorSpace);

	params.fading = std::clamp(context.parser.parse(context.options, L"FadingRatio").valueOr(0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(context.parser.parse(context.options, L"borderSize").valueOr(0), 0, params.length / 2);

	params.stationary = context.parser.parse(context.options, L"stationary").valueOr(false);

	params.silenceThreshold = context.parser.parse(context.options, L"silenceThreshold").valueOr(-70.0f);
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

std::pair<std::vector<Spectrogram::ColorDescription>, std::vector<float>>
Spectrogram::parseColors(const OptionList& list, Color::Mode defaultColorSpace, option_parsing::OptionParser& parser, Logger& cl) {
	std::vector<ColorDescription> resultColors;
	std::vector<float> levels;

	float prevValue = -std::numeric_limits<float>::infinity();

	bool first = true;
	for (auto colorsDescription : list) {
		auto [valueOpt, colorOpt] = colorsDescription.breakFirst(L':');

		if (colorOpt.empty()) {
			cl.error(L"description doesn't contain color: {}", colorsDescription.asString());
			throw InvalidOptionsException{};
		}

		const auto value = parser.parse(valueOpt, colorsDescription.asString()).as<float>();

		if (value <= prevValue) {
			cl.error(L"values {} and {}: values must be increasing", prevValue, value);
			throw InvalidOptionsException{};
		}
		if (MyMath::checkFloatEqual(value, prevValue, 1024.0f)) {
			cl.error(L"values {} and {} are too close", prevValue, value);
			throw InvalidOptionsException{};
		}

		levels.push_back(value);
		const auto color = parser.parse(colorOpt, L"colors").asCustom<Color>(defaultColorSpace);
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

	return { std::move(resultColors), std::move(levels) };
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

	context.printer.print(L"{}{}.bmp", snapshot.folder, context.filePrefix);

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, context.printer.getBufferView());
	snapshot.writeNeeded = false;
}

bool Spectrogram::getProp(
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
