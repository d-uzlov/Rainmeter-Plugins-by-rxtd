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

using namespace std::string_literals;

using namespace audio_analyzer;

bool Spectrogram::vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
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

bool Spectrogram::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	void* paramsPtr,
	index legacyNumber
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceName = om.get(L"source").asIString();
	if (params.sourceName.empty()) {
		cl.error(L"source not found");
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

	if (om.has(L"colors")) {
		auto [modeOption, desc] = om.get(L"colors").breakFirst(L' ');
		auto colorsDescriptionList = desc.asList(L';');

		ColorMixMode mode;
		if (auto mixMode = modeOption.asIString(L"rgb");
			mixMode == L"rgb") {
			mode = ColorMixMode::eRGB;
		} else if (mixMode == L"hsv") {
			mode = ColorMixMode::eHSV;
		} else {
			cl.error(L"unknown color mode '{}', using rgb instead", mixMode);
			mode = ColorMixMode::eRGB;
		}

		float prevValue = -std::numeric_limits<float>::infinity();

		params.colorMinValue = std::numeric_limits<float>::infinity();
		params.colorMaxValue = -std::numeric_limits<float>::infinity();

		for (index i = 0; i < colorsDescriptionList.size(); i++) {
			auto [valueOpt, colorOpt] = colorsDescriptionList.get(i).breakFirst(L':');

			if (colorOpt.empty()) {
				cl.error(L"colors: color #{} is not found");
				continue;
			}

			float value = valueOpt.asFloatF();

			if (value <= prevValue) {
				cl.error(L"colors: values {} and {}: values must be increasing", prevValue, value);
				return { };
			}
			if (value / prevValue < 1.001f && value - prevValue < 0.001f) {
				cl.error(L"colors: values {} and {} are too close, discarding second one", prevValue, value);
				continue;
			}

			params.colorLevels.push_back(value);
			auto color = colorOpt.asColor();
			if (mode == ColorMixMode::eRGB) {
				color = color.rgb2hsv();
			}
			params.colors.push_back(Params::ColorDescription{ 0.0f, color });
			if (i > 0) {
				params.colors[i - 1].widthInverted = 1.0f / (value - prevValue);
			}

			prevValue = value;
			params.colorMinValue = std::min(params.colorMinValue, value);
			params.colorMaxValue = std::max(params.colorMaxValue, value);
		}

		if (params.colors.size() < 2) {
			cl.error(L"need at least 2 colors but {} found", params.colors.size());
			params.colors = { };
		}
	} else {
		params.colors.resize(2);
		params.colors[0].color = om.get(L"baseColor").asColor({ 0, 0, 0, 1 }).rgb2hsv();
		params.colors[1].color = om.get(L"maxColor").asColor({ 1, 1, 1, 1 }).rgb2hsv();
		params.colorMinValue = 0.0f;
		params.colorMaxValue = 1.0f;
	}

	if (auto mixMode = om.get(L"mixMode").asIString(L"rgb");
		mixMode == L"rgb") {
		params.mixMode = ColorMixMode::eRGB;
	} else if (mixMode == L"hsv") {
		params.mixMode = ColorMixMode::eHSV;
	} else {
		cl.error(L"unknown mixMode '{}', using rgb instead", mixMode);
		params.mixMode = ColorMixMode::eRGB;
	}

	if (params.mixMode == ColorMixMode::eRGB) {
		for (auto& desc : params.colors) {
			desc.color = desc.color.hsv2rgb();
		}
	}

	params.borderColor = om.get(L"borderColor").asColor({ 1.0, 0.2, 0.2, 1 });

	params.fading = std::clamp(om.get(L"fadingPercent").asFloat(0.0), 0.0, 1.0);

	params.borderSize = std::clamp<index>(om.get(L"borderSize").asInt(0), 0, params.length / 2);

	params.stationary = om.get(L"stationary").asBool(false);

	return true;
}

SoundHandler::LinkingResult Spectrogram::vFinishLinking(Logger& cl) {
	const auto dataSize = getSource()->getDataSize();

	image.setBackground(params.colors[0].color.hsv2rgb().toIntColor());
	image.setStationary(params.stationary);

	sifh.setBorderSize(params.borderSize);
	sifh.setColors(params.colors[0].color, params.borderColor);
	sifh.setFading(params.fading);

	image.setDimensions(params.length, dataSize.valuesCount);
	stripBuffer.resize(dataSize.valuesCount);
	intBuffer.resize(dataSize.valuesCount);

	filepath = params.prefix;
	filepath += getChannel().technicalName();
	filepath += L".bmp";

	blockSize = index(getSampleRate() * params.resolution);

	return { 0, 0 };
}

void Spectrogram::fillStrip(array_view<float> data, array_span<utils::Color> buffer) const {
	const utils::LinearInterpolatorF interpolator{ params.colorMinValue, params.colorMaxValue, 0.0, 1.0 };
	const auto lowColor = params.colors[0].color;
	const auto highColor = params.colors[1].color;

	for (index i = 0; i < index(buffer.size()); ++i) {
		auto value = interpolator.toValue(data[i]);
		value = std::clamp(value, 0.0f, 1.0f);

		buffer[i] = (highColor * value + lowColor * (1.0f - value));
	}
}

void Spectrogram::fillStripMulticolor(array_view<float> data, array_span<utils::Color> buffer) const {
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

		buffer[i] = (highColor * percentValue + lowColor * (1.0f - percentValue));
	}
}

void Spectrogram::vProcess(array_view<float> wave) {
	if (blockSize <= 0) {
		return;
	}

	const index waveSize = wave.size();
	counter += waveSize;

	if (counter < blockSize) {
		return;
	}

	auto& source = *getSource();

	source.finish();
	const auto sd = source.getData();

	// todo check id
	const auto data = source.getData().values[0];

	const bool dataIsZero = std::all_of(
		data.begin(),
		data.end(),
		[=](auto x) { return x < params.colorMinValue; }
	);

	while (counter >= blockSize) {
		changed = true;

		if (dataIsZero) {
			if (params.mixMode == ColorMixMode::eRGB) {
				image.pushEmptyStrip(params.colors[0].color.toIntColor());
			} else {
				image.pushEmptyStrip(params.colors[0].color.hsv2rgb().toIntColor());
			}
		} else {
			if (params.colors.size() == 2) {
				// only use 2 colors
				fillStrip(data, stripBuffer);
			} else {
				// many colors, but slightly slower
				fillStripMulticolor(data, stripBuffer);
			}

			if (params.mixMode == ColorMixMode::eHSV) {
				for (auto& color : stripBuffer) {
					color = color.hsv2rgb();
				}
			}

			for (index i = 0; i < index(stripBuffer.size()); i++) {
				intBuffer[i] = stripBuffer[i].toIntColor();
			}

			image.pushStrip(intBuffer);
		}

		counter -= blockSize;
	}
}

void Spectrogram::vFinish() {
	if (!changed) {
		return;
	}

	if (params.fading != 0.0) {
		if (image.isForced() || !writerHelper.isEmptinessWritten()) {
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
