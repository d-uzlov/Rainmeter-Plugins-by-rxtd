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

		double prevValue = -std::numeric_limits<double>::infinity();

		bool colorsAreBroken = false;

		params.colorMinValue = std::numeric_limits<double>::infinity();
		params.colorMaxValue = -std::numeric_limits<double>::infinity();

		for (index i = 0; i < colorsDescriptionList.size(); i++) {
			auto [valueOpt, colorOpt] = colorsDescriptionList.get(i).breakFirst(L' ');

			float value = valueOpt.asFloat();

			if (value <= prevValue) {
				cl.error(L"Colors: values {} and {}: values must be increasing", prevValue, value);
				colorsAreBroken = true;
				break;
			}
			if (value / prevValue < 1.001 && value - prevValue < 0.001) {
				cl.error(L"Colors: values {} and {} are too close, discarding second one", prevValue, value);
				continue;
			}

			params.colorLevels.push_back(value);
			params.colors.push_back(Params::ColorDescription{ 0.0, colorOpt.asColor() });
			if (i > 0) {
				params.colors[i - 1].widthInverted = 1.0 / (value - prevValue);
			}

			prevValue = value;
			params.colorMinValue = std::min<double>(params.colorMinValue, value);
			params.colorMaxValue = std::max<double>(params.colorMaxValue, value);
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
		params.colorMinValue = 0.0;
		params.colorMaxValue = 1.0;
	}

	params.borderColor = optionMap.get(L"borderColor"sv).asColor({ 1.0, 0.2, 0.2, 1 });

	if (const auto fading = optionMap.get(L"fading").asIString(L"None");
		fading == L"None") {
		params.fading = FD::eNONE;
	} else if (fading == L"Linear") {
		params.fading = FD::eLINEAR;
	} else if (fading == L"Pow2") {
		params.fading = FD::ePOW2;
	} else if (fading == L"Pow4") {
		params.fading = FD::ePOW4;
	} else if (fading == L"Pow8") {
		params.fading = FD::ePOW8;
	} else {
		cl.warning(L"fading '{}' is not recognized, assume 'None'", fading);
		params.fading = FD::eNONE;
	}

	params.borderSize = std::max(optionMap.get(L"borderSize"sv).asInt(0), 0);

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

	image.setBackground(params.baseColor.toInt());
	image.setWidth(params.length);
	image.setStationary(params.stationary);

	sifh.setBorderSize(params.borderSize);
	sifh.setColors(params.colors[0].color, params.borderColor);
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

void Spectrogram::fillStrip(array_view<float> data) {
	auto& strip = stripBuffer;
	utils::LinearInterpolator interpolator{ params.colorMinValue, params.colorMaxValue, 0.0, 1.0 };

	for (index i = 0; i < index(strip.size()); ++i) {
		double value = data[i];
		value = interpolator.toValue(value);
		value = std::clamp(value, 0.0, 1.0);

		auto color = params.baseColor * (1.0 - value) + params.maxColor * value;

		strip[i] = color.toInt();
	}
}

void Spectrogram::fillStripMulticolor(array_view<float> data) {
	auto& strip = stripBuffer;

	for (index i = 0; i < index(strip.size()); ++i) {
		const double value = std::clamp<double>(data[i], params.colorMinValue, params.colorMaxValue);

		index lowColorIndex = 0;
		for (index j = 1; j < index(params.colors.size()); j++) {
			const double colorHighValue = params.colorLevels[j];
			if (value <= colorHighValue) {
				lowColorIndex = j - 1;
				break;
			}
		}

		const double lowColorValue = params.colorLevels[lowColorIndex];
		const double intervalCoef = params.colors[lowColorIndex].widthInverted;
		const auto lowColor = params.colors[lowColorIndex].color;
		const auto highColor = params.colors[lowColorIndex + 1].color;

		const double percentValue = (value - lowColorValue) * intervalCoef;

		const auto color = lowColor * (1.0 - percentValue) + highColor * percentValue;

		strip[i] = color.toInt();
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
			image.pushEmptyLine(params.baseColor.toInt());
		} else {
			if (params.colors.empty()) {
				// only use 2 colors
				fillStrip(data);
			} else {
				// many colors, but slightly slower
				fillStripMulticolor(data);
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

	if (params.borderSize > 0 || params.fading != utils::StripedImageFadeHelper::FadingType::eNONE) {
		if (!writerHelper.isEmptinessWritten()) {
			sifh.setLastStripIndex(image.getLastStripIndex());
			sifh.inflate(image.getPixels());
		}
		writerHelper.write(sifh.getResultBuffer(), !image.isForced(), filepath);
	} else {
		writerHelper.write(image.getPixels(), image.isEmpty(), filepath);
	}

	changed = false;
}
