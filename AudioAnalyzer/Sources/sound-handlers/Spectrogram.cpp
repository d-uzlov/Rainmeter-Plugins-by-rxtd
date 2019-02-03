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

	return params;
}

void Spectrogram::setSamplesPerSec(index samplesPerSec) {
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
	auto index = lastIndex + 1;
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

		buffer[lastIndex][i] = color.toInt();
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


	while (counter >= blockSize) {
		changed = true;

		lastIndex++;
		if (lastIndex >= params.length) {
			lastIndex = 0;
		}

		fillLine(data);

		counter -= blockSize;
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
