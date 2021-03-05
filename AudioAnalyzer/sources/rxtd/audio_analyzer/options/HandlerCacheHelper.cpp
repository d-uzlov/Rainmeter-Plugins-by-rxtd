// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "HandlerCacheHelper.h"

#include "rxtd/audio_analyzer/sound_processing/sound_handlers/basic-handlers//BlockHandler.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/basic-handlers//Loudness.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/basic-handlers//WaveForm.h"

#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/BandCascadeTransformer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/BandResampler.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/FftAnalyzer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/SingleValueTransformer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/Spectrogram.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/TimeResampler.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/spectrum-stack/UniformBlur.h"

#include "rxtd/option_parsing/OptionMap.h"

using namespace std::string_literals;

using rxtd::audio_analyzer::options::HandlerCacheHelper;
using rxtd::audio_analyzer::options::HandlerInfo;
using rxtd::audio_analyzer::handler::HandlerBase;

HandlerInfo* HandlerCacheHelper::getHandlerInfo(const istring& name, Logger& cl) {
	auto& val = patchersCache[name];

	if (!val.updated) {
		val = parseHandler(name % csView(), std::move(val), cl);
		val.updated = true;
	}

	return val.valid ? &val.info : nullptr;
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

	try {
		val.info.meta = createHandlerPatcher(optionMap, cl);
		val.valid = true;
	} catch (HandlerBase::InvalidOptionsException&) {
		// function that threw the exception must have handled all log messages about reasons
		val.valid = false;
	}

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
) const noexcept(false) {
	using namespace handler;

	const auto type = optionMap.get(L"type").asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		throw HandlerBase::InvalidOptionsException{};
	}

	HandlerBase::ParamParseContext parseContext{ optionMap, cl, rain, version, *parserPtr };

	if (type == L"rms") {
		return HandlerBase::createMetaForClass<BlockRms>(parseContext);
	}
	if (type == L"peak") {
		return HandlerBase::createMetaForClass<BlockPeak>(parseContext);
	}
	if (type == L"fft") {
		return HandlerBase::createMetaForClass<FftAnalyzer>(parseContext);
	}
	if (type == L"BandResampler") {
		return HandlerBase::createMetaForClass<BandResampler>(parseContext);
	}
	if (type == L"BandCascadeTransformer") {
		return HandlerBase::createMetaForClass<BandCascadeTransformer>(parseContext);
	}
	if (type == L"UniformBlur") {
		return HandlerBase::createMetaForClass<UniformBlur>(parseContext);
	}
	if (type == L"spectrogram") {
		return HandlerBase::createMetaForClass<Spectrogram>(parseContext);
	}
	if (type == L"waveform") {
		return HandlerBase::createMetaForClass<WaveForm>(parseContext);
	}
	if (type == L"loudness") {
		return HandlerBase::createMetaForClass<Loudness>(parseContext);
	}
	if (type == L"ValueTransformer") {
		return HandlerBase::createMetaForClass<SingleValueTransformer>(parseContext);
	}
	if (type == L"TimeResampler") {
		return HandlerBase::createMetaForClass<TimeResampler>(parseContext);
	}

	cl.error(L"unknown type '{}'", type);
	throw HandlerBase::InvalidOptionsException{};
}

void HandlerCacheHelper::readRawDescription2(
	isview type,
	const OptionMap& optionMap,
	string& rawDescription2
) const {
	if (type == L"BandResampler") {
		auto freqListIndex = optionMap.get(L"freqList").asString();
		if (!freqListIndex.empty()) {
			string freqListOptionName = L"FreqList-"s += freqListIndex;
			auto freqListOption = rain.read(freqListOptionName);
			if (freqListOption.empty()) {
				freqListOptionName = L"FreqList_"s += freqListIndex;
				freqListOption = rain.read(freqListOptionName);
			}
			rawDescription2 = freqListOption.asString();
		}
	}
}
