/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WaveForm.h"
#include <filesystem>

#include "rxtd/std_fixes/MyMath.h"

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::WaveForm;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

using namespace std::string_literals;
using rxtd::audio_analyzer::image_utils::Color;

ParamsContainer WaveForm::vParseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	Version version
) const {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	params.width = om.get(L"width").asInt(100);
	if (params.width < 2) {
		cl.error(L"width must be >= 2 but {} found", params.width);
		return {};
	}

	params.height = om.get(L"height").asInt(100);
	if (params.height < 2) {
		cl.error(L"height must be >= 2 but {} found", params.height);
		return {};
	}

	if (params.width * params.height > 1000 * 1000) {
		cl.warning(L"dangerously big width and height: {}, {}", params.width, params.height);
	}

	params.resolution = om.get(L"resolution").asFloat(50);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 100", params.resolution);
		params.resolution = 100;
	}
	params.resolution *= 0.001;

	params.folder = rain.getPathFromCurrent(om.get(L"folder").asString() % own());

	params.colors.background = Color::parse(om.get(L"backgroundColor").asString(), { 0, 0, 0 }).toIntColor();
	params.colors.wave = Color::parse(om.get(L"waveColor").asString(), { 1, 1, 1 }).toIntColor();
	params.colors.line = Color::parse(om.get(L"lineColor").asString(), { 0.5, 0.5, 0.5, 0.5 }).toIntColor();
	params.colors.border = Color::parse(om.get(L"borderColor").asString(), { 1.0, 0.2, 0.2 }).toIntColor();

	if (const auto ldpString = om.get(L"lineDrawingPolicy").asIString(L"always");
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

	params.stationary = om.get(L"stationary").asBool(false);
	params.connected = om.get(L"connected").asBool(true);

	params.borderSize = om.get(L"borderSize").asInt(0);
	params.borderSize = std::clamp<index>(params.borderSize, 0, params.width / 2);

	params.lineThickness = om.get(L"lineThickness").asInt(2 - (params.height & 1));
	params.lineThickness = std::clamp<index>(params.lineThickness, 0, params.height);

	params.fading = om.get(L"FadingRatio").asFloat(0.0);

	params.silenceThreshold = om.get(L"silenceThreshold").asFloatF(-70);
	params.silenceThreshold = MyMath::db2amplitude(params.silenceThreshold);

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = CVT::parse(om.get(L"transform").asString(), transformLogger);

	return result;
}

HandlerBase::ConfigurationResult
WaveForm::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;
	auto blockSize = static_cast<index>(static_cast<double>(sampleRate) * params.resolution);
	blockSize = std::max<index>(blockSize, 1);

	minDistinguishableValue = 1.0 / static_cast<double>(params.height);

	drawer.setImageParams(params.width, params.height, params.stationary);
	drawer.setParams(
		params.connected, params.borderSize, params.fading,
		params.lineDrawingPolicy, params.lineThickness,
		params.colors
	);

	drawer.inflate();

	mainCounter.reset();
	originalCounter.reset();

	mainCounter.setBlockSize(blockSize);
	originalCounter.setBlockSize(blockSize);

	auto& snapshot = externalData.clear<Snapshot>();

	snapshot.prefix = params.folder;

	snapshot.blockSize = blockSize;

	snapshot.pixels.setBuffersCount(params.height);
	snapshot.pixels.setBufferSize(params.width);

	snapshot.pixels.copyWithResize(drawer.getResultBuffer());

	snapshot.writeNeeded = true;
	snapshot.empty = false;

	return { 0, {} };
}

void WaveForm::vProcess(ProcessContext context, ExternalData& externalData) {
	const bool wasEmpty = drawer.isEmpty();

	bool anyChanges = false;

	mainCounter.setWave(context.wave);
	originalCounter.setWave(context.originalWave);
	while (true) {
		mainCounter.update();
		originalCounter.update();

		if (!mainCounter.isReady()) {
			break;
		}

		if (originalCounter.isBelowThreshold(params.silenceThreshold)) {
			drawer.fillSilence();
		} else {
			const auto transformedMin = std::abs(params.transformer.apply(std::abs(mainCounter.getMin())));
			const auto transformedMax = std::abs(params.transformer.apply(std::abs(mainCounter.getMax())));
			drawer.fillStrip(
				std::copysign(transformedMin, mainCounter.getMin()),
				std::copysign(transformedMax, mainCounter.getMax())
			);
		}

		anyChanges = true;
		mainCounter.reset();
		originalCounter.reset();
	}

	if (anyChanges) {
		drawer.inflate();

		auto& snapshot = externalData.cast<Snapshot>();
		snapshot.writeNeeded = true;
		snapshot.empty = drawer.isEmpty();

		snapshot.pixels.copyWithResize(drawer.getResultBuffer());
	}
}

void WaveForm::staticFinisher(const Snapshot& snapshot, const ExternalMethods::CallContext& context) {
	auto& writeNeeded = snapshot.writeNeeded;
	if (!writeNeeded) {
		return;
	}

	snapshot.filenameBuffer = snapshot.prefix;
	snapshot.filenameBuffer += context.filePrefix;
	snapshot.filenameBuffer += L".bmp";

	snapshot.writerHelper.write(snapshot.pixels, snapshot.empty, snapshot.filenameBuffer);
	writeNeeded = false;
}

bool WaveForm::getProp(
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
