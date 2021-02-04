/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HandlerCacheHelper.h"

#include "sound-processing/sound-handlers/BlockHandler.h"
#include "sound-processing/sound-handlers/WaveForm.h"
#include "sound-processing/sound-handlers/Loudness.h"

#include "sound-processing/sound-handlers/spectrum-stack/FftAnalyzer.h"
#include "sound-processing/sound-handlers/spectrum-stack/BandResampler.h"
#include "sound-processing/sound-handlers/spectrum-stack/BandCascadeTransformer.h"
#include "sound-processing/sound-handlers/spectrum-stack/UniformBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/Spectrogram.h"
#include "sound-processing/sound-handlers/spectrum-stack/SingleValueTransformer.h"
#include "sound-processing/sound-handlers/spectrum-stack/TimeResampler.h"

#include "sound-processing/sound-handlers/spectrum-stack/legacy_WeightedBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_FiniteTimeFilter.h"

#include "option-parsing/OptionMap.h"

using namespace std::string_literals;

using namespace audio_analyzer;

PatchInfo* HandlerCacheHelper::getHandler(const istring& name) {
	auto& info = patchersCache[name];

	if (!info.updated) {
		info = parseHandler(name % csView(), std::move(info));
		info.updated = true;
	}

	return info.patchInfo.fun == nullptr ? nullptr : &info.patchInfo;
}

HandlerCacheHelper::HandlerRawInfo HandlerCacheHelper::parseHandler(sview name, HandlerRawInfo handler) {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);
	if (descriptionOption.empty()) {
		optionName = L"Handler_"s += name;
		descriptionOption = rain.read(optionName);
	}

	auto cl = rain.createLogger().context(L"Handler '{}': ", name);

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		return {};
	}

	OptionMap optionMap = descriptionOption.asMap(L'|', L' ');
	string rawDescription2;
	readRawDescription2(optionMap.get(L"type").asIString(), optionMap, rawDescription2);

	if (handler.patchInfo.rawDescription == descriptionOption.asString()
		&& handler.patchInfo.rawDescription2 == rawDescription2) {
		return handler;
	}
	anythingChanged = true;

	handler.patchInfo = createHandlerPatcher(optionMap, cl);
	handler.patchInfo.type = optionMap.get(L"type").asIString();

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: '{}'", unusedOptions);
	}

	handler.patchInfo.rawDescription2 = std::move(rawDescription2);
	handler.patchInfo.rawDescription = descriptionOption.asString();

	return handler;
}

PatchInfo HandlerCacheHelper::createHandlerPatcher(
	const OptionMap& optionMap,
	Logger& cl
) const {
	const auto type = optionMap.get(L"type").asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		return {};
	}

	if (type == L"rms") {
		return createPatcherT<BlockRms>(optionMap, cl);
	}
	if (type == L"peak") {
		return createPatcherT<BlockPeak>(optionMap, cl);
	}
	if (type == L"fft") {
		return createPatcherT<FftAnalyzer>(optionMap, cl);
	}
	if (type == L"BandResampler") {
		return createPatcherT<BandResampler>(optionMap, cl);
	}
	if (type == L"BandCascadeTransformer") {
		return createPatcherT<BandCascadeTransformer>(optionMap, cl);
	}
	if (type == L"UniformBlur") {
		return createPatcherT<UniformBlur>(optionMap, cl);
	}
	if (type == L"spectrogram") {
		return createPatcherT<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform") {
		return createPatcherT<WaveForm>(optionMap, cl);
	}
	if (type == L"loudness") {
		return createPatcherT<Loudness>(optionMap, cl);
	}
	if (type == L"ValueTransformer") {
		return createPatcherT<SingleValueTransformer>(optionMap, cl);
	}
	if (type == L"TimeResampler") {
		return createPatcherT<TimeResampler>(optionMap, cl);
	}

	if (type == L"WeightedBlur") {
		if (!(version < Version::eVERSION2)) {
			cl.warning(L"handler WeightedBlur is deprecated");
		}
		return createPatcherT<legacy_WeightedBlur>(optionMap, cl);
	}
	if (type == L"FiniteTimeFilter") {
		if (!(version < Version::eVERSION2)) {
			cl.warning(L"handler FiniteTimeFilter is deprecated");
		}
		return createPatcherT<legacy_FiniteTimeFilter>(optionMap, cl);
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
		return createPatcherT<SingleValueTransformer>(optionMapLocal, cl);
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
