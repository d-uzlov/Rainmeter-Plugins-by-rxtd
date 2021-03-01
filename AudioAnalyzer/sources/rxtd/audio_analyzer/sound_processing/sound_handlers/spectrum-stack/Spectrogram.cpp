/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Spectrogram.h"

#include <filesystem>

#include "rxtd/LinearInterpolator.h"
#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/std_fixes/MyMath.h"

using namespace std::string_literals;

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::Spectrogram;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer Spectrogram::vParseParams(ParamParseContext& context) const noexcept(false) {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	const auto sourceId = context.options.get(L"source").asIString();
	if (sourceId.empty()) {
		context.log.error(L"source is not found");
		throw InvalidOptionsException{};
	}

	params.length = context.options.get(L"length").asInt(100);
	if (params.length < 2) {
		context.log.error(L"length must be >= 2 but {} found", params.length);
		throw InvalidOptionsException{};
	}
	if (params.length >= 1500) {
		context.log.warning(L"dangerously large length {}", params.length);
	}

	params.resolution = context.options.get(L"resolution").asFloat(50);
	if (params.resolution <= 0) {
		context.log.warning(L"resolution must be > 0 but {} found. Assume 50", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.folder = context.rain.getPathFromCurrent(context.options.get(L"folder").asString() % own());

	using MixMode = Color::Mode;
	if (auto mixMode = context.options.get(L"mixMode").asIString(L"srgb");
		mixMode == L"srgb") {
		params.mixMode = MixMode::eRGB;
	} else if (mixMode == L"hsv") {
		params.mixMode = MixMode::eHSV;
	} else if (mixMode == L"hsl") {
		params.mixMode = MixMode::eHSL;
	} else if (mixMode == L"ycbcr") {
		params.mixMode = MixMode::eRGB; // difference between ycbcr and rgb is linear
	} else {
		context.log.error(L"unknown mixMode '{}', using rgb instead", mixMode);
		params.mixMode = MixMode::eRGB;
	}

	if (!context.options.get(L"colors").empty()) {
		auto colorsDescriptionList = context.options.get(L"colors").asList(L';');

		float prevValue = -std::numeric_limits<float>::infinity();

		bool first = true;
		for (auto colorsDescription : colorsDescriptionList) {
			auto [valueOpt, colorOpt] = colorsDescription.breakFirst(L':');

			if (colorOpt.empty()) {
				context.log.warning(L"colors: description '{}' doesn't contain color", colorsDescription.asString());
				continue;
			}

			float value = valueOpt.asFloatF();

			if (value <= prevValue) {
				context.log.error(L"colors: values {} and {}: values must be increasing", prevValue, value);
				throw InvalidOptionsException{};
			}
			if (value / prevValue < 1.001f && value - prevValue < 0.001f) {
				context.log.warning(L"colors: values {} and {} are too close, discarding second one", prevValue, value);
				continue;
			}

			params.colorLevels.push_back(value);
			const auto color = Color::parse(colorOpt.asString()).convert(params.mixMode);
			if (first) {
				first = false;
			} else {
				params.colors.back().widthInverted = 1.0f / (value - prevValue);
			}
			params.colors.push_back(ColorDescription{ 0.0f, color });

			prevValue = value;
		}

		if (params.colors.size() < 2) {
			context.log.error(L"need at least 2 colors but {} found", params.colors.size());
			throw InvalidOptionsException{};
		}
	} else {
		params.colors.resize(2);
		params.colors[0].color = Color::parse(context.options.get(L"baseColor").asString(), { 0, 0, 0 }).convert(params.mixMode);
		params.colors[1].color = Color::parse(context.options.get(L"maxColor").asString(), { 1, 1, 1 }).convert(params.mixMode);
		params.colorLevels.push_back(0.0f);
		params.colorLevels.push_back(1.0f);
	}

	params.borderColor = Color::parse(context.options.get(L"borderColor").asString(), { 1.0, 0.2, 0.2 });

	params.fading = std::clamp(context.options.get(L"FadingRatio").asFloat(0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(context.options.get(L"borderSize").asInt(0), 0, params.length / 2);

	params.stationary = context.options.get(L"stationary").asBool(false);

	params.silenceThreshold = context.options.get(L"silenceThreshold").asFloatF(-70);
	params.silenceThreshold = MyMath::db2amplitude(params.silenceThreshold);

	return result;
}

HandlerBase::ConfigurationResult
Spectrogram::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	blockSize = static_cast<index>(static_cast<double>(sampleRate) * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	const index width = params.length;
	const index height = getConfiguration().sourcePtr->getDataSize().valuesCount;

	const auto backgroundIntColor = params.colors[0].color.toIntColor();
	image.setParams(width, height, backgroundIntColor, params.stationary);

	fadeHelper.setParams(backgroundIntColor, params.borderSize, params.borderColor.toIntColor(), params.fading);

	if (params.fading != 0.0) {
		fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
		fadeHelper.inflate(image.getPixels());
	} else {
		if (params.borderSize != 0) {
			fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
			fadeHelper.drawBorderInPlace(image.getPixelsWritable());
		}
	}
	imageHasChanged = true;

	ism.setParams(
		blockSize,
		config.sourcePtr->getDataSize().eqWaveSizes[0],
		height,
		params.colors, params.colorLevels
	);

	minMaxCounter.reset();
	minMaxCounter.setBlockSize(blockSize);

	auto& snapshot = externalData.clear<Snapshot>();

	snapshot.prefix = params.folder;

	snapshot.blockSize = blockSize;

	snapshot.pixels.copyWithResize(params.fading != 0.0 ? fadeHelper.getResultBuffer() : image.getPixels());

	snapshot.writeNeeded = true;
	snapshot.empty = false;


	return { 0, {} };
}

void Spectrogram::vProcess(ProcessContext context, ExternalData& externalData) {
	minMaxCounter.setWave(context.originalWave);

	auto& source = *getConfiguration().sourcePtr;

	const bool imageWasEmpty = image.isEmpty();

	ism.setChunks(source.getChunks(0));

	while (true) {
		minMaxCounter.update();

		if (!minMaxCounter.isReady()) {
			break;
		}

		ism.next();
		imageHasChanged = true;

		if (minMaxCounter.isBelowThreshold(params.silenceThreshold)) {
			image.pushEmptyStrip(params.colors[0].color.toIntColor());
		} else {
			image.pushStrip(ism.getBuffer());
		}
		minMaxCounter.reset();
	}

	if (imageHasChanged) {
		if (params.fading != 0.0) {
			if (!image.isEmpty() || (image.isEmpty() && !imageWasEmpty)) {
				fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
				fadeHelper.inflate(image.getPixels());
			}
		} else {
			if (params.borderSize != 0) {
				fadeHelper.setPastLastStripIndex(image.getPastLastStripIndex());
				fadeHelper.drawBorderInPlace(image.getPixelsWritable());
			}
		}

		auto& snapshot = externalData.cast<Snapshot>();
		snapshot.writeNeeded = true;
		snapshot.empty = image.isEmpty();

		snapshot.pixels.copyWithResize(params.fading != 0.0 ? fadeHelper.getResultBuffer() : image.getPixels());

		imageHasChanged = false;
	}
}

void Spectrogram::staticFinisher(const Snapshot& snapshot, const ExternalMethods::CallContext& context) {
	if (!snapshot.writeNeeded) {
		return;
	}

	snapshot.filenameBuffer = snapshot.prefix;
	snapshot.filenameBuffer += context.filePrefix;
	snapshot.filenameBuffer += L".bmp";

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, snapshot.filenameBuffer);
	snapshot.writeNeeded = false;
}

void Spectrogram::InputStripMaker::fillStrip(array_view<float> data, array_span<IntColor> buffer) const {
	const LinearInterpolator<float> interpolator{ colorLevels.front(), colorLevels.back(), 0.0f, 1.0f };
	const auto lowColor = colors[0].color;
	const auto highColor = colors[1].color;

	for (index i = 0; i < static_cast<index>(buffer.size()); ++i) {
		auto value = interpolator.toValue(data[i]);
		value = std::clamp(value, 0.0f, 1.0f);

		buffer[i] = (highColor * value + lowColor * (1.0f - value)).toIntColor();
	}
}

void Spectrogram::InputStripMaker::fillStripMulticolor(
	array_view<float> data, array_span<IntColor> buffer
) const {
	for (index i = 0; i < buffer.size(); ++i) {
		const auto value = std::clamp(data[i], colorLevels.front(), colorLevels.back());

		index lowColorIndex = 0;
		for (index j = 1; j < colors.size(); j++) {
			const auto colorHighValue = colorLevels[j];
			if (value <= colorHighValue) {
				lowColorIndex = j - 1;
				break;
			}
		}

		const auto lowColorValue = colorLevels[lowColorIndex];
		const auto intervalCoef = colors[lowColorIndex].widthInverted;
		const auto lowColor = colors[lowColorIndex].color;
		const auto highColor = colors[lowColorIndex + 1].color;

		const float percentValue = (value - lowColorValue) * intervalCoef;

		buffer[i] = (highColor * percentValue + lowColor * (1.0f - percentValue)).toIntColor();
	}
}

bool Spectrogram::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternalMethods::CallContext& context
) {
	if (prop == L"file") {
		snapshot.filenameBuffer = snapshot.prefix;
		snapshot.filenameBuffer += context.filePrefix;
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
