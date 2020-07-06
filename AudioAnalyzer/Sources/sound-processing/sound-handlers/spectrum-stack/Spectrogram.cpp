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

#include "undef.h"
#include "MutableLinearInterpolator.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void Spectrogram::setParams(const Params& _params, Channel channel) {
	if (this->params == _params) {
		return;
	}

	// if (params.prefix != _params.prefix) {
	// 	// this ensures that if there is a silence
	// 	// and there is already an image in the .prefix folder
	// 	// then we will properly show empty image instead of that old image
	// 	buffer.init(params.baseColor.toInt());
	// }

	this->params = _params;

	filepath = params.prefix;
	filepath += L"spectrogram-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

	utils::FileWrapper::createDirectories(params.prefix);

	image.setBackground(params.baseColor.toInt());
	image.setImageHeight(params.length);

	updateParams();
}

std::optional<Spectrogram::Params> Spectrogram::parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl, const utils::Rainmeter& rain) {
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

	string folder { optionMap.get(L"folder"sv).asString() };
	if (!folder.empty() && folder[0] == L'\"') {
		folder = folder.substr(1);
	}
	std::filesystem::path path { folder };
	if (!path.is_absolute()) {
		folder = rain.replaceVariables(L"[#CURRENTPATH]") % own() + folder;
	}
	folder = std::filesystem::absolute(folder).wstring();
	folder = LR"(\\?\)"s + folder;
	if (!folder.empty() && folder[folder.size() - 1] != L'\\') {
		folder += L'\\';
	}

	params.prefix = folder;

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
			params.colors.push_back(Params::ColorDescription { 0.0, colorOpt.asColor() });
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

	return params;
}

void Spectrogram::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	updateParams();
}

const wchar_t* Spectrogram::getProp(const isview& prop) const {
	if (prop == L"file") {
		return filepath.c_str();
	} else if (prop == L"block size") {
		propString = std::to_wstring(blockSize);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void Spectrogram::updateParams() {
	blockSize = index(samplesPerSec * params.resolution);
}

void Spectrogram::fillLine(array_view<float> data) {
	auto line = image.nextLine();
	utils::MutableLinearInterpolator interpolator { params.colorMinValue, params.colorMaxValue, 0.0, 1.0 };

	for (index i = 0; i < line.size(); ++i) {
		double value = data[i];
		value = interpolator.toValue(value);
		value = std::clamp(value, 0.0, 1.0);

		auto color = params.baseColor * (1.0 - value) + params.maxColor * value;

		line[i] = color.toInt();
	}
}

void Spectrogram::fillLineMulticolor(array_view<float> data) {
	auto line = image.nextLine();

	for (index i = 0; i < line.size(); ++i) {
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

		auto color = lowColor * (1.0 - percentValue) + highColor * percentValue;

		line[i] = color.toInt();
	}
}

void Spectrogram::process(const DataSupplier& dataSupplier) {
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
		return;
	}

	const auto data = source->getData(0);
	const auto dataSize = data.size();
	image.setImageWidth(dataSize);

	const bool dataIsZero = std::all_of(data.data(), data.data() + dataSize, [=](auto x) { return x < params.colorMinValue; });

	while (counter >= blockSize) {
		changed = true;

		if (dataIsZero) {
			image.fillNextLine(params.baseColor.toInt());
		} else if (params.colors.empty()) { // only use 2 colors
			fillLine(data);
		} else { // many colors, but slightly slower
			fillLineMulticolor(data);
		}

		counter -= blockSize;
	}

}

void Spectrogram::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void Spectrogram::finish(const DataSupplier& dataSupplier) {
	if (changed) {
		image.writeTransposed(filepath);
		changed = false;
	}
}
