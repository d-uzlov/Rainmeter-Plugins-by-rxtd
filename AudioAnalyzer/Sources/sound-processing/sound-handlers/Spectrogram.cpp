/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Spectrogram.h"
#include "BmpWriter.h"
#include <filesystem>
#include "windows-wrappers/FileWrapper.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void Spectrogram::setParams(const Params& _params) {
	if (this->params == _params) {
		return;
	}

	this->params = _params;

	filepath.clear();

	utils::FileWrapper::createDirectories(params.prefix);

	updateParams();
}

std::optional<Spectrogram::Params> Spectrogram::parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl, const utils::Rainmeter& rain) {
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

	params.baseColor = optionMap.get(L"baseColor"sv).asColor({ 0, 0, 0, 1 });
	params.maxColor = optionMap.get(L"maxColor"sv).asColor({ 1, 1, 1, 1 });
	if (optionMap.has(L"colors"sv)) {
		utils::OptionParser parser { };

		auto colorsDescriptions = optionMap.get(L"colors"sv).asString();
		auto colorsDescriptionList = parser.asList(colorsDescriptions, L';');

		double prevValue = std::numeric_limits<double>::infinity();

		bool colorsAreBroken = false;

		params.colorMinValue = std::numeric_limits<double>::infinity();
		params.colorMaxValue = -std::numeric_limits<double>::infinity();

		for (index i = 0; i < colorsDescriptionList.size(); i++) {
			auto [valueOpt, colorOpt] = colorsDescriptionList.getOption(i).breakFirst(L' ');

			float value = valueOpt.asFloat();
			utils::Color color = colorOpt.asColor();

			if (i > 0) {
				if (value <= prevValue) {
					cl.error(L"Colors: values {} and {}: values must be increasing", prevValue, value);
					continue;
				}
				if (value / prevValue < 1.001 && value - prevValue < 0.001) {
					cl.error(L"Colors: values {} and {} are too close, discarding second one", prevValue, value);
					continue;
				}
			}

			params.colorLevels.push_back(value);
			params.colors.push_back(Params::ColorDescription { 0.0, color });
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
		// TODO optimize for 2 colors - like, do all as usual 2 colors but shift bound from [0, 1] to [min, max]
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

void Spectrogram::reset() {
}

void Spectrogram::updateParams() {
	blockSize = index(samplesPerSec * params.resolution);
	buffer.setBuffersCount(params.length);

	reset();
}

void Spectrogram::writeFile(const DataSupplier& dataSupplier) {
	auto index = lastLineIndex + 1;
	if (index >= params.length) {
		index = 0;
	}

	auto width = sourceSize;
	auto height = params.length;
	auto writeBufferSize = width * height;
	auto writeBuffer = dataSupplier.getBuffer<uint32_t>(writeBufferSize);
	utils::BmpWriter::writeFile(filepath, buffer[0].data(), width, height, index, writeBuffer.data(), writeBufferSize); // TODO remove .data()
}

void Spectrogram::fillLine(array_view<float> data) {
	for (index i = 0; i < sourceSize; ++i) {
		double value = data[i];
		value = std::clamp(value, 0.0, 1.0);

		auto color = params.baseColor * (1.0 - value) + params.maxColor * value;

		buffer[lastLineIndex][i] = color.toInt();
	}
}

void Spectrogram::fillLineMulticolor(array_view<float> data) {
	for (index i = 0; i < sourceSize; ++i) {
		const double value = std::clamp<double>(data[i], params.colorMinValue, params.colorMaxValue);

		index lowColorIndex = 0;
		for (index j = 1; j < params.colors.size(); j++) {
			const double colorLowValue = params.colorLevels[j];
			lowColorIndex = j - 1;
			if (value <= colorLowValue) {
				break;
			}
		}

		const double lowColorValue = params.colorLevels[lowColorIndex];
		const double intervalCoef = params.colors[lowColorIndex].widthInverted;
		const auto lowColor = params.colors[lowColorIndex].color;
		const auto highColor = params.colors[lowColorIndex + 1].color;

		const double percentValue = (value - lowColorValue) * intervalCoef;

		auto color = lowColor * (1.0 - percentValue) + highColor * percentValue;

		buffer[lastLineIndex][i] = color.toInt();
	}
}

void Spectrogram::process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0) {
		return;
	}

	const auto source = dataSupplier.getHandler(params.sourceName);
	if (source == nullptr) {
		return;
	}

	const auto data = source->getData(0);
	const auto dataSize = data.size();

	const bool dataIsZero = std::all_of(data.data(), data.data() + dataSize, [=](auto x) { return x < params.colorMinValue; });
	if (!dataIsZero) {
		lastNonZeroLine = 0;
	} else {
		lastNonZeroLine++;
	}
	if (lastNonZeroLine > params.length) {
		lastNonZeroLine = params.length;
		counter = blockSize;
		return;
	}

	if (dataSize != sourceSize) {
		sourceSize = dataSize;
		buffer.setBufferSize(dataSize);
		std::fill_n(buffer[0].data(), dataSize * params.length, params.baseColor.toInt());
	}

	if (filepath.empty()) {
		filepath = params.prefix;
		filepath += L"spectrogram-";
		filepath += dataSupplier.getChannel().technicalName();
		filepath += L".bmp"sv;
	}

	const auto waveSize = dataSupplier.getWaveSize();
	counter += waveSize;

	if (params.colors.empty()) { // only use 2 colors
		while (counter >= blockSize) {
			changed = true;

			lastLineIndex++;
			if (lastLineIndex >= params.length) {
				lastLineIndex = 0;
			}

			fillLine(data);

			counter -= blockSize;
		}
	} else { // many colors, but slightly slower
		while (counter >= blockSize) {
			changed = true;

			lastLineIndex++;
			if (lastLineIndex >= params.length) {
				lastLineIndex = 0;
			}

			fillLineMulticolor(data);

			counter -= blockSize;
		}
	}
}

void Spectrogram::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void Spectrogram::finish(const DataSupplier& dataSupplier) {
	if (changed) {
		writeFile(dataSupplier);
		changed = false;
	}
}
