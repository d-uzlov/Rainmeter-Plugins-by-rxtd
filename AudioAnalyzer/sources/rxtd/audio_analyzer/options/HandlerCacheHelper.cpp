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
using MapValue = HandlerCacheHelper::MapValue;
using rxtd::audio_analyzer::options::HandlerInfo;
using rxtd::audio_analyzer::handler::HandlerBase;

const MapValue& HandlerCacheHelper::getHandlerInfo(const istring& name, isview source, Logger& cl) const {
	auto& val = patchersCache[name];

	if (!val.updated) {
		try {
			const bool changed = parseHandler(name % csView(), source, val.info, cl);
			if (changed) {
				val.valid = true;
				val.changed = true;
			}
		} catch (HandlerBase::InvalidOptionsException&) {
			// function that threw the exception must have handled all log messages about reasons
			val.valid = false;
		}
		val.updated = true;
	}

	return val;
}

bool HandlerCacheHelper::parseHandler(sview name, isview source, HandlerInfo& info, Logger& cl) const {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		throw HandlerBase::InvalidOptionsException{};
	}

	OptionMap optionMap = descriptionOption.asMap(L'|', L' ');

	if (info.rawDescription == descriptionOption.asString() && info.source == source) {
		return {};
	}

	info.rawDescription = descriptionOption.asString();
	info.source = source;

	info.type = optionMap.get(L"type").asIString();
	info.meta = createHandlerPatcher(info.type, optionMap, cl);
	if (info.meta.sourcesCount == 0) {
		if (!source.empty()) {
			cl.error(L"{} can't have source", info.type);
			throw HandlerBase::InvalidOptionsException{};
		}
	} else if (info.meta.sourcesCount == 1) {
		if (source.empty()) {
			cl.error(L"{} must have source", info.type);
			throw HandlerBase::InvalidOptionsException{};
		}
	} else {
		cl.error(L"too many sources requested, tell the plugin dev about this error", info.type);
		throw HandlerBase::InvalidOptionsException{};
	}

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: {}", unusedOptions);
	}

	return true;
}

HandlerBase::HandlerMetaInfo HandlerCacheHelper::createHandlerPatcher(
	isview type,
	const OptionMap& optionMap,
	Logger& cl
) const noexcept(false) {
	using namespace handler;

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
	if (type == L"loudness") {
		return HandlerBase::createMetaForClass<Loudness>(parseContext);
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
	if (type == L"ValueTransformer") {
		return HandlerBase::createMetaForClass<SingleValueTransformer>(parseContext);
	}
	if (type == L"TimeResampler") {
		return HandlerBase::createMetaForClass<TimeResampler>(parseContext);
	}

	if (type == L"spectrogram") {
		return HandlerBase::createMetaForClass<Spectrogram>(parseContext);
	}
	if (type == L"waveform") {
		return HandlerBase::createMetaForClass<WaveForm>(parseContext);
	}

	cl.error(L"unknown type: {}", type);
	throw HandlerBase::InvalidOptionsException{};
}
