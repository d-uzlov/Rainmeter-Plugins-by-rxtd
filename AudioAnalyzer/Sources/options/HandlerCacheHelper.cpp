/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HandlerCacheHelper.h"

#include "AudioChild.h"
#include "sound-processing/sound-handlers/basic-handlers//BlockHandler.h"
#include "sound-processing/sound-handlers/basic-handlers//Loudness.h"
#include "sound-processing/sound-handlers/basic-handlers//WaveForm.h"

#include "sound-processing/sound-handlers/spectrum-stack/BandCascadeTransformer.h"
#include "sound-processing/sound-handlers/spectrum-stack/BandResampler.h"
#include "sound-processing/sound-handlers/spectrum-stack/FftAnalyzer.h"
#include "sound-processing/sound-handlers/spectrum-stack/SingleValueTransformer.h"
#include "sound-processing/sound-handlers/spectrum-stack/Spectrogram.h"
#include "sound-processing/sound-handlers/spectrum-stack/TimeResampler.h"
#include "sound-processing/sound-handlers/spectrum-stack/UniformBlur.h"

#include "sound-processing/sound-handlers/spectrum-stack/legacy_FiniteTimeFilter.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_WeightedBlur.h"

#include "option-parsing/OptionMap.h"

using namespace std::string_literals;

using rxtd::audio_analyzer::HandlerCacheHelper;
using rxtd::audio_analyzer::HandlerInfo;
using rxtd::audio_analyzer::handler::HandlerBase;

HandlerInfo* HandlerCacheHelper::getHandlerInfo(const istring& name, Logger& cl) {
	auto& val = patchersCache[name];

	if (!val.updated) {
		val = parseHandler(name % csView(), std::move(val), cl);
		val.updated = true;
	}

	return val.info.meta.valid ? &val.info : nullptr;
}

HandlerCacheHelper::MapValue HandlerCacheHelper::parseHandler(sview name, MapValue val, Logger& cl) {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);
	if (descriptionOption.empty()) {
		optionName = L"Handler_"s += name;
		descriptionOption = rain.read(optionName);
	}

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		return {};
	}

	OptionMap optionMap = descriptionOption.asMap(L'|', L' ');
	string rawDescription2;
	readRawDescription2(optionMap.get(L"type").asIString(), optionMap, rawDescription2);

	if (val.info.rawDescription == descriptionOption.asString()
		&& val.info.rawDescription2 == rawDescription2) {
		return val;
	}

	anythingChanged = true;

	val.info.meta = createHandlerPatcher(optionMap, cl);

	val.info.sources.clear();
	for (auto opt : optionMap.get(L"source").asList(L',')) {
		val.info.sources.push_back(opt.asIString() % own());
	}
	val.info.type = optionMap.get(L"type").asIString();

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: '{}'", unusedOptions);
	}

	val.info.rawDescription2 = std::move(rawDescription2);
	val.info.rawDescription = descriptionOption.asString();

	return val;
}

HandlerBase::HandlerMetaInfo HandlerCacheHelper::createHandlerPatcher(
	const OptionMap& optionMap,
	Logger& cl
) const {
	using namespace handler;

	const auto type = optionMap.get(L"type").asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		return {};
	}

	if (type == L"rms") {
		return HandlerBase::createMetaForClass<BlockRms>(optionMap, cl, rain, version);
	}
	if (type == L"peak") {
		return HandlerBase::createMetaForClass<BlockPeak>(optionMap, cl, rain, version);
	}
	if (type == L"fft") {
		return HandlerBase::createMetaForClass<FftAnalyzer>(optionMap, cl, rain, version);
	}
	if (type == L"BandResampler") {
		return HandlerBase::createMetaForClass<BandResampler>(optionMap, cl, rain, version);
	}
	if (type == L"BandCascadeTransformer") {
		return HandlerBase::createMetaForClass<BandCascadeTransformer>(optionMap, cl, rain, version);
	}
	if (type == L"UniformBlur") {
		return HandlerBase::createMetaForClass<UniformBlur>(optionMap, cl, rain, version);
	}
	if (type == L"spectrogram") {
		return HandlerBase::createMetaForClass<Spectrogram>(optionMap, cl, rain, version);
	}
	if (type == L"waveform") {
		return HandlerBase::createMetaForClass<WaveForm>(optionMap, cl, rain, version);
	}
	if (type == L"loudness") {
		return HandlerBase::createMetaForClass<Loudness>(optionMap, cl, rain, version);
	}
	if (type == L"ValueTransformer") {
		return HandlerBase::createMetaForClass<SingleValueTransformer>(optionMap, cl, rain, version);
	}
	if (type == L"TimeResampler") {
		return HandlerBase::createMetaForClass<TimeResampler>(optionMap, cl, rain, version);
	}

	if (type == L"WeightedBlur") {
		if (!(version < Version::eVERSION2)) {
			cl.warning(L"handler WeightedBlur is deprecated");
		}
		return HandlerBase::createMetaForClass<legacy_WeightedBlur>(optionMap, cl, rain, version);
	}
	if (type == L"FiniteTimeFilter") {
		if (!(version < Version::eVERSION2)) {
			cl.warning(L"handler FiniteTimeFilter is deprecated");
		}
		return HandlerBase::createMetaForClass<legacy_FiniteTimeFilter>(optionMap, cl, rain, version);
	}

	if (type == L"LogarithmicValueMapper") {
		if (!(version < Version::eVERSION2)) {
			cl.warning(L"handler LogarithmicValueMapper is deprecated");
		}

		common::buffer_printer::BufferPrinter bp;
		bp.append(L"type ValueTransformer");
		bp.append(L"| source {}", optionMap.get(L"source").asString());
		const double sensitivity = std::clamp<double>(
			optionMap.get(L"sensitivity").asFloat(35.0),
			std::numeric_limits<float>::epsilon(),
			1000.0
		);
		const double offset = optionMap.get(L"offset").asFloatF(0.0);

		bp.append(L"| transform db map[from -{} : 0, to {} : {}]", sensitivity * 0.5, offset, 1.0 + offset);
		auto optionMapLocal = common::options::Option{ bp.getBufferView() }.asMap(L'|', L' ');
		return HandlerBase::createMetaForClass<SingleValueTransformer>(optionMapLocal, cl, rain, version);
	}

	cl.error(L"unknown type '{}'", type);
	return {};
}

void HandlerCacheHelper::readRawDescription2(
	isview type,
	const OptionMap& optionMap,
	string& rawDescription2
) const {
	if (type == L"BandResampler") {
		auto freqListIndex = optionMap.get(L"freqList").asString();
		if (!freqListIndex.empty()) {
			auto freqListOptionName = L"FreqList-"s += freqListIndex;
			auto freqListOption = rain.read(freqListOptionName);
			if (freqListOption.empty()) {
				freqListOptionName = L"FreqList_"s += freqListIndex;
				freqListOption = rain.read(freqListOptionName);
			}
			rawDescription2 = freqListOption.asString();
		}
	}
}
