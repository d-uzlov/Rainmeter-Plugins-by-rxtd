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

#include "IntMixer.h"
#include "windows-wrappers/FileWrapper.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionList.h"
#include "LinearInterpolator.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<Spectrogram::Params>
Spectrogram::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain) {
	Params params;

	params.sourceName = optionMap.get(L"source"sv).asIString();
	if (params.sourceName.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.length = optionMap.get(L"length"sv).asInt(100);
	if (params.length < 2) {
		cl.error(L"length must be >= 2 but {} found", params.length);
		return std::nullopt;
	}
	if (params.length >= 1500) {
		cl.warning(L"dangerously large length {}", params.length);
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

	if (optionMap.has(L"colors"sv)) {
		auto colorsDescriptionList = optionMap.get(L"colors"sv).asList(L';');

		float prevValue = -std::numeric_limits<float>::infinity();

		bool colorsAreBroken = false;

		params.colorMinValue = std::numeric_limits<float>::infinity();
		params.colorMaxValue = -std::numeric_limits<float>::infinity();

		for (index i = 0; i < colorsDescriptionList.size(); i++) {
			auto [valueOpt, colorOpt] = colorsDescriptionList.get(i).breakFirst(L' ');

			float value = valueOpt.asFloatF();

			if (value <= prevValue) {
				cl.error(L"Colors: values {} and {}: values must be increasing", prevValue, value);
				colorsAreBroken = true;
				break;
			}
			if (value / prevValue < 1.001f && value - prevValue < 0.001f) {
				cl.error(L"Colors: values {} and {} are too close, discarding second one", prevValue, value);
				continue;
			}

			params.colorLevels.push_back(value);
			params.colors.push_back(Params::ColorDescription{ 0.0f, colorOpt.asColor().toIntColor() });
			if (i > 0) {
				params.colors[i - 1].widthInverted = 1.0f / (value - prevValue);
			}

			prevValue = value;
			params.colorMinValue = std::min(params.colorMinValue, value);
			params.colorMaxValue = std::max(params.colorMaxValue, value);
		}

		if (!colorsAreBroken && params.colors.size() < 2) {
			cl.error(L"Not enough colors found: {}", params.colors.size());
			params.colors = { };
		}

		if (params.colors.size() == 2) {
			// optimize for 2-colors case
			params.baseColor = params.colors[0].color;
			params.maxColor = params.colors[1].color;
			params.colors = { };
		} else {
			// base color is used for background
			params.baseColor = params.colors[0].color;
		}
	} else {
		params.baseColor = optionMap.get(L"baseColor"sv).asColor({ 0, 0, 0, 1 });
		params.maxColor = optionMap.get(L"maxColor"sv).asColor({ 1, 1, 1, 1 });
		params.colorMinValue = 0.0f;
		params.colorMaxValue = 1.0f;
	}

	params.borderColor = optionMap.get(L"borderColor"sv).asColor({ 1.0, 0.2, 0.2, 1 });

	params.fading = std::clamp(optionMap.get(L"fadingPercent").asFloat(0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(optionMap.get(L"borderSize"sv).asInt(0), 0, params.length / 2);

	params.stationary = optionMap.get(L"stationary").asBool(false);

	return params;
}

void Spectrogram::setParams(const Params& _params, Channel channel) {
	if (params == _params) {
		return;
	}

	params = _params;

	filepath = params.folder;
	filepath += L"spectrogram-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

	image.setBackground(params.baseColor.toIntColor());
	image.setWidth(params.length);
	image.setStationary(params.stationary);

	sifh.setBorderSize(params.borderSize);
	if (params.colors.empty()) {
		sifh.setColors(params.baseColor, params.borderColor);
	} else {
		// TODO
		sifh.setColors(params.colors[0].color, params.borderColor);
	}
	sifh.setFading(params.fading);

	updateParams();
}

void Spectrogram::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	updateParams();
}

bool Spectrogram::getProp(const isview& prop, utils::BufferPrinter& printer) const {
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

void Spectrogram::updateParams() {
	blockSize = index(samplesPerSec * params.resolution);
}

void Spectrogram::fillStrip(array_view<float> data, array_span<utils::IntColor> buffer) const {
	utils::LinearInterpolatorF interpolator{ params.colorMinValue, params.colorMaxValue, 0.0, 1.0 };
	utils::IntColor baseColor = params.baseColor.toIntColor();
	utils::IntColor maxColor = params.maxColor.toIntColor();

	for (index i = 0; i < index(buffer.size()); ++i) {
		auto value = interpolator.toValue(data[i]);
		value = std::clamp(value, 0.0f, 1.0f);
		utils::IntMixer mixer{ value };

		// auto color = params.baseColor * (1.0f - value) + params.maxColor * value;
		buffer[i] = maxColor.mixWith(baseColor, mixer);
	}
}

void Spectrogram::fillStripMulticolor(array_view<float> data, array_span<utils::IntColor> buffer) const {
	for (index i = 0; i < index(buffer.size()); ++i) {
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
		
		utils::IntMixer mixer { percentValue };
		buffer[i] = highColor.mixWith(lowColor, mixer);
	}
}

void Spectrogram::_process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0) {
		return;
	}

	const auto waveSize = dataSupplier.getWave().size();
	counter += waveSize;

	if (counter < blockSize) {
		return;
	}

	const auto source = dataSupplier.getHandler(params.sourceName);
	if (source == nullptr) {
		setValid(false);
		return;
	}

	const auto data = source->getData(0);
	const auto dataSize = data.size();
	image.setHeight(dataSize);
	stripBuffer.resize(dataSize);

	const bool dataIsZero = std::all_of(
		data.data(),
		data.data() + dataSize,
		[=](auto x) { return x < params.colorMinValue; }
	);

	while (counter >= blockSize) {
		changed = true;

		if (dataIsZero) {
			image.pushEmptyStrip(params.baseColor.toIntColor());
		} else {
			if (params.colors.empty()) {
				// only use 2 colors
				fillStrip(data, stripBuffer);
			} else {
				// many colors, but slightly slower
				fillStripMulticolor(data, stripBuffer);
			}

			image.pushStrip(stripBuffer);
		}

		counter -= blockSize;
	}
}

void Spectrogram::_finish(const DataSupplier& dataSupplier) {
	if (!changed) {
		return;
	}

	if (params.fading != 0.0) {
		if (!writerHelper.isEmptinessWritten()) {
			sifh.setPastLastStripIndex(image.getPastLastStripIndex());
			sifh.inflate(image.getPixels());
		}
		writerHelper.write(sifh.getResultBuffer(), !image.isForced(), filepath);
	} else {
		if (params.borderSize != 0) {
			sifh.setPastLastStripIndex(image.getPastLastStripIndex());
			sifh.drawBorderInPlace(image.getPixelsWritable());
		}
		auto pixels = image.getPixels();
		utils::array2d_view<uint32_t> buffer{
			&(pixels[0].data()->full), pixels.getBuffersCount(), pixels.getBufferSize()
		};
		writerHelper.write(buffer, !image.isForced(), filepath);
	}

	changed = false;
}
