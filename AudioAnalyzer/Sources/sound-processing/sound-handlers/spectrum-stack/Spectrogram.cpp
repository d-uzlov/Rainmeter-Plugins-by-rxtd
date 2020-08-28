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

#include "windows-wrappers/FileWrapper.h"
#include "option-parser/OptionList.h"
#include "LinearInterpolator.h"

using namespace std::string_literals;

using namespace audio_analyzer;

using utils::Color;

SoundHandler::ParseResult Spectrogram::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return { };
	}

	params.length = om.get(L"length").asInt(100);
	if (params.length < 2) {
		cl.error(L"length must be >= 2 but {} found", params.length);
		return { };
	}
	if (params.length >= 1500) {
		cl.warning(L"dangerously large length {}", params.length);
	}

	params.resolution = om.get(L"resolution").asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 50", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.prefix = utils::FileWrapper::getAbsolutePath(
		om.get(L"folder").asString() % own(),
		rain.replaceVariables(L"[#CURRENTPATH]") % own()
	);
	params.prefix += L"spectrogram-";

	using MixMode = Color::Mode;
	if (auto mixMode = om.get(L"mixMode").asIString(L"srgb");
		mixMode == L"srgb") {
		params.mixMode = MixMode::eRGB;
	} else if (mixMode == L"hsv") {
		params.mixMode = MixMode::eHSV;
	} else if (mixMode == L"hsl") {
		params.mixMode = MixMode::eHSL;
	} else if (mixMode == L"ycbcr") {
		params.mixMode = MixMode::eRGB; // difference between ycbcr and rgb is linear
	} else {
		cl.error(L"unknown mixMode '{}', using rgb instead", mixMode);
		params.mixMode = MixMode::eRGB;
	}

	if (!om.get(L"colors").empty()) {
		auto colorsDescriptionList = om.get(L"colors").asList(L';');

		float prevValue = -std::numeric_limits<float>::infinity();

		params.colorMinValue = std::numeric_limits<float>::infinity();
		params.colorMaxValue = -std::numeric_limits<float>::infinity();

		bool first = true;
		for (auto colorsDescription : colorsDescriptionList) {
			auto [valueOpt, colorOpt] = colorsDescription.breakFirst(L':');

			if (colorOpt.empty()) {
				cl.warning(L"colors: description '{}' doesn't contain color", colorsDescription.asString());
				continue;
			}

			float value = valueOpt.asFloatF();

			if (value <= prevValue) {
				cl.error(L"colors: values {} and {}: values must be increasing", prevValue, value);
				return { };
			}
			if (value / prevValue < 1.001f && value - prevValue < 0.001f) {
				cl.warning(L"colors: values {} and {} are too close, discarding second one", prevValue, value);
				continue;
			}

			params.colorLevels.push_back(value);
			const auto color = Color::parse(colorOpt.asString()).convert(params.mixMode);
			if (first) {
				first = false;
			} else {
				params.colors.back().widthInverted = 1.0f / (value - prevValue);
			}
			params.colors.push_back(Params::ColorDescription{ 0.0f, color });

			prevValue = value;
			params.colorMinValue = std::min(params.colorMinValue, value);
			params.colorMaxValue = std::max(params.colorMaxValue, value);
		}

		if (params.colors.size() < 2) {
			cl.error(L"need at least 2 colors but {} found", params.colors.size());
			return { };
		}
	} else {
		params.colors.resize(2);
		params.colors[0].color = Color::parse(om.get(L"baseColor").asString(), { 0, 0, 0 }).convert(params.mixMode);
		params.colors[1].color = Color::parse(om.get(L"maxColor").asString(), { 1, 1, 1 }).convert(params.mixMode);
		params.colorMinValue = 0.0f;
		params.colorMaxValue = 1.0f;
	}

	params.borderColor = Color::parse(om.get(L"borderColor").asString(), { 1.0, 0.2, 0.2 });

	params.fading = std::clamp(om.get(L"fadingPercent").asFloat(0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(om.get(L"borderSize").asInt(0), 0, params.length / 2);

	params.stationary = om.get(L"stationary").asBool(false);

	ParseResult result{ true };
	result.params = std::move(params);
	result.finisher = staticFinisher;
	result.propGetter = getProp;
	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult Spectrogram::vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	blockSize = index(sampleRate * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	const index width = params.length;
	const index height = getConfiguration().sourcePtr->getDataSize().valuesCount;

	const auto backgroundIntColor = params.colors[0].color.toIntColor();
	image.setParams(width, height, backgroundIntColor, params.stationary);

	sifh.setParams(backgroundIntColor, params.borderSize, params.borderColor.toIntColor(), params.fading);

	if (params.fading != 0.0) {
		if (image.isForced() || !writerHelper.isEmptinessWritten()) {
			sifh.setPastLastStripIndex(image.getPastLastStripIndex());
			sifh.inflate(image.getPixels());
		}
	} else {
		if (params.borderSize != 0) {
			sifh.setPastLastStripIndex(image.getPastLastStripIndex());
			sifh.drawBorderInPlace(image.getPixelsWritable());
		}
	}

	stripBuffer.resize(height);

	filepath = params.prefix;
	filepath += config.channelName;
	filepath += L".bmp";

	std::fill(stripBuffer.begin(), stripBuffer.end(), backgroundIntColor);

	counter = 0;
	dataShortageEqSize = 0;
	overpushCount = 0;
	writeNeeded = true;


	if (nullptr == std::any_cast<Snapshot>(&snapshotAny)) {
		snapshotAny = Snapshot{ };
	}
	auto& snapshot = *std::any_cast<Snapshot>(&snapshotAny);

	snapshot.filepath = filepath;
	snapshot.blockSize = blockSize;

	snapshot.pixels.setBuffersCount(height);
	snapshot.pixels.setBufferSize(width);

	auto pixels = params.fading != 0.0 ? sifh.getResultBuffer().getFlat() : image.getPixels().getFlat();
	std::copy(pixels.begin(), pixels.end(), snapshot.pixels.getFlat().begin());

	snapshot.writeNeeded = true;
	snapshot.empty = false;


	return { 0, 0 };
}

void Spectrogram::vProcess(array_view<float> wave, clock::time_point killTime) {
	if (image.isForced()) {
		image.removeLast(overpushCount);
	} else {
		dataShortageEqSize -= blockSize * overpushCount;
	}
	overpushCount = 0;
	dataShortageEqSize += wave.size();

	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;

	bool imageChanged = false;
	for (auto chunk : source.getChunks(0)) {
		counter += chunk.equivalentWaveSize;

		if (counter < blockSize || clock::now() > killTime) {
			continue;
		}

		imageChanged = true;

		const auto data = chunk.data;

		lastDataIsZero = *std::max_element(data.begin(), data.end()) < params.colorMinValue;

		if (!lastDataIsZero) {
			if (params.colors.size() == 2) {
				// only use 2 colors
				fillStrip(data, stripBuffer);
			} else {
				// many colors, but slightly slower
				fillStripMulticolor(data, stripBuffer);
			}
		}

		while (counter >= blockSize) {
			if (lastDataIsZero) {
				image.pushEmptyStrip(params.colors[0].color.toIntColor());
			} else {
				image.pushStrip(stripBuffer);
			}

			counter -= blockSize;
			dataShortageEqSize -= blockSize;
		}
	}

	// in case kill time is exceeded, this ensures that image speed is consistent
	while (counter >= blockSize) {
		if (lastDataIsZero) {
			image.pushEmptyStrip(params.colors[0].color.toIntColor());
		} else {
			image.pushStrip(stripBuffer);
		}

		counter -= blockSize;
		dataShortageEqSize -= blockSize;
	}

	index localShortageSize = dataShortageEqSize;
	while (localShortageSize >= blockSize && overpushCount < params.length) {
		imageChanged = true;

		if (lastDataIsZero) {
			image.pushEmptyStrip(params.colors[0].color.toIntColor());
		} else {
			image.pushStrip(stripBuffer);
		}

		localShortageSize -= blockSize;
		overpushCount++;
	}

	if (imageChanged) {
		writeNeeded = true;

		if (params.fading != 0.0) {
			if (image.isForced() || !writerHelper.isEmptinessWritten()) {
				sifh.setPastLastStripIndex(image.getPastLastStripIndex());
				sifh.inflate(image.getPixels());
			}
		} else {
			if (params.borderSize != 0) {
				sifh.setPastLastStripIndex(image.getPastLastStripIndex());
				sifh.drawBorderInPlace(image.getPixelsWritable());
			}
		}
	}
}

void Spectrogram::vUpdateSnapshot(std::any& handlerSpecificData) const {
	if (!writeNeeded) {
		return;
	}

	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);
	snapshot.writeNeeded = true;
	snapshot.empty = !image.isForced();

	auto pixels = params.fading != 0.0 ? sifh.getResultBuffer().getFlat() : image.getPixels().getFlat();
	std::copy(pixels.begin(), pixels.end(), snapshot.pixels.getFlat().begin());

	writeNeeded = false;
}

void Spectrogram::staticFinisher(std::any& handlerSpecificData) {
	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);

	auto& writeNeeded = snapshot.writeNeeded;
	if (!writeNeeded) {
		return;
	}

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, snapshot.filepath);
	writeNeeded = false;
}

void Spectrogram::fillStrip(array_view<float> data, array_span<utils::IntColor> buffer) const {
	const utils::LinearInterpolatorF interpolator{ params.colorMinValue, params.colorMaxValue, 0.0, 1.0 };
	const auto lowColor = params.colors[0].color;
	const auto highColor = params.colors[1].color;

	for (index i = 0; i < index(buffer.size()); ++i) {
		auto value = interpolator.toValue(data[i]);
		value = std::clamp(value, 0.0f, 1.0f);

		buffer[i] = (highColor * value + lowColor * (1.0f - value)).toIntColor();
	}
}

void Spectrogram::fillStripMulticolor(array_view<float> data, array_span<utils::IntColor> buffer) const {
	for (index i = 0; i < buffer.size(); ++i) {
		const auto value = std::clamp(data[i], params.colorMinValue, params.colorMaxValue);

		index lowColorIndex = 0;
		for (index j = 1; j < index(params.colors.size()); j++) {
			const auto colorHighValue = params.colorLevels[j];
			if (value <= colorHighValue) {
				lowColorIndex = j - 1;
				break;
			}
		}

		const auto lowColorValue = params.colorLevels[lowColorIndex];
		const auto intervalCoef = params.colors[lowColorIndex].widthInverted;
		const auto lowColor = params.colors[lowColorIndex].color;
		const auto highColor = params.colors[lowColorIndex + 1].color;

		const float percentValue = (value - lowColorValue) * intervalCoef;

		buffer[i] = (highColor * percentValue + lowColor * (1.0f - percentValue)).toIntColor();
	}
}

bool Spectrogram::getProp(const std::any& handlerSpecificData, isview prop, utils::BufferPrinter& printer) {
	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);

	if (prop == L"file") {
		printer.print(snapshot.filepath);
		return true;
	}
	if (prop == L"block size") {
		printer.print(snapshot.blockSize);
		return true;
	}

	return false;
}
