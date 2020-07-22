/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveForm.h"
#include <filesystem>
#include "windows-wrappers/FileWrapper.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<WaveForm::Params> WaveForm::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl,
	const utils::Rainmeter& rain
) {
	Params params;

	params.width = optionMap.get(L"width"sv).asInt(100);
	if (params.width < 2) {
		cl.error(L"width must be >= 2 but {} found", params.width);
		return std::nullopt;
	}

	params.height = optionMap.get(L"height"sv).asInt(100);
	if (params.height < 2) {
		cl.error(L"height must be >= 2 but {} found", params.height);
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

	params.colors.background = optionMap.get(L"backgroundColor"sv).asColor({ 0, 0, 0, 1 });
	params.colors.wave = optionMap.get(L"waveColor"sv).asColor({ 1, 1, 1, 1 });
	params.colors.line = optionMap.get(L"lineColor"sv).asColor({ 0.5, 0.5, 0.5, 0.5 });
	params.colors.border = optionMap.get(L"borderColor"sv).asColor({ 1.0, 0.2, 0.2, 1 });
	params.colors.halo = optionMap.get(L"haloColor"sv).asColor({ 1.0, 0.4, 0.0, 1 });

	if (const auto ldpString = optionMap.get(L"lineDrawingPolicy"sv).asIString(L"always");
		ldpString == L"always") {
		params.lineDrawingPolicy = LDP::eALWAYS;
	} else if (ldpString == L"belowWave") {
		params.lineDrawingPolicy = LDP::eBELOW_WAVE;
	} else if (ldpString == L"never") {
		params.lineDrawingPolicy = LDP::eNEVER;
	} else {
		cl.warning(L"lineDrawingPolicy '{}' is not recognized, assume 'always'", ldpString);
		params.lineDrawingPolicy = LDP::eALWAYS;
	}

	if (const auto edgesString = optionMap.get(L"Edges"sv).asIString(L"none");
		edgesString == L"none") {
		params.edges = SE::eNONE;
	} else if (edgesString == L"smoothMinMax") {
		params.edges = SE::eMIN_MAX;
	} else if (edgesString == L"halo") {
		params.edges = SE::eHALO;
	} else {
		cl.warning(L"SmoothEdges '{}' is not recognized, assume 'none'", edgesString);
		params.edges = SE::eNONE;
	}

	params.stationary = optionMap.get(L"Stationary").asBool(false);
	params.connected = optionMap.get(L"connected").asBool(true);

	params.borderSize = optionMap.get(L"borderSize").asInt(0);

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

	// legacy
	if (optionMap.has(L"transform"sv)) {
		params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), cl);
	} else {
		const auto gain = optionMap.get(L"gain"sv).asFloat(1.0);
		utils::BufferPrinter printer;
		printer.print(L"map[0,1][0,{}]", gain);
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ printer.getBufferView() }, cl);
	}

	return params;
}

double WaveForm::WaveformValueTransformer::apply(double value) {
	const bool positive = value > 0;
	value = cvt.apply(std::abs(value));

	if (!positive) {
		value *= -1;
	}

	return value;
}

void WaveForm::WaveformValueTransformer::updateTransformations(index samplesPerSec, index blockSize) {
	cvt.setParams(samplesPerSec, blockSize);
}

void WaveForm::WaveformValueTransformer::reset() {
	cvt.resetState();
}

void WaveForm::setParams(const Params& _params, Channel channel) {
	if (params == _params) {
		return;
	}

	this->params = _params;

	if (_params.width <= 0 || _params.height <= 0) {
		return;
	}

	minDistinguishableValue = 1.0 / params.height / 2.0; // below half pixel

	drawer.setDimensions(params.width, params.height);
	drawer.setEdges(params.edges);
	drawer.setColors(params.colors);
	drawer.setLineDrawingPolicy(params.lineDrawingPolicy);
	drawer.setStationary(params.stationary);
	drawer.setFading(params.fading);
	drawer.setConnected(params.connected);
	drawer.setBorderSize(params.borderSize);

	filepath = params.folder;
	filepath += L"wave-";
	filepath += channel.technicalName();
	filepath += L".bmp"sv;

	minTransformer = { params.transformer };
	maxTransformer = { params.transformer };

	updateParams();
}

void WaveForm::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;

	updateParams();
}

bool WaveForm::getProp(const isview& prop, utils::BufferPrinter& printer) const {
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

void WaveForm::reset() {
	counter = 0;
	min = 10.0;
	max = -10.0;

	minTransformer.reset();
	maxTransformer.reset();
}

void WaveForm::updateParams() {
	blockSize = index(samplesPerSec * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minTransformer.updateTransformations(samplesPerSec, blockSize);
	maxTransformer.updateTransformations(samplesPerSec, blockSize);

	reset();
}

void WaveForm::pushStrip(double min, double max) {
	min = minTransformer.apply(min);
	max = maxTransformer.apply(max);

	if (std::abs(min) < minDistinguishableValue && std::abs(max) < minDistinguishableValue) {
		drawer.fillSilence();
	} else {
		drawer.fillStrip(min, max);
	}

	changed = true;
}

void WaveForm::_process(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	const auto wave = dataSupplier.getWave();

	for (const auto value : wave) {
		min = std::min<double>(min, value);
		max = std::max<double>(max, value);
		counter++;
		if (counter >= blockSize) {
			pushStrip(min, max);

			counter = 0;
			min = 10.0;
			max = -10.0;
		}
	}
}

void WaveForm::_processSilence(const DataSupplier& dataSupplier) {
	if (blockSize <= 0 || params.width <= 0 || params.height <= 0) {
		return;
	}

	const auto waveSize = dataSupplier.getWave().size();

	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const auto blockRemaining = blockSize - counter;
		min = std::min<double>(min, 0.0);
		max = std::max<double>(max, 0.0);

		if (waveProcessed + blockRemaining <= waveSize) {
			pushStrip(min, max);

			waveProcessed += blockRemaining;
			counter = 0;
			min = 10.0;
			max = -10.0;
		} else {
			counter = counter + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void WaveForm::_finish(const DataSupplier& dataSupplier) {
	if (changed) {
		drawer.inflate();
		writerHelper.write(drawer.getResultBuffer(), drawer.isEmpty(), filepath);
		changed = false;
	}
}
