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
#include "sound-processing/sound-handlers/spectrum-stack/WeightedBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_FiniteTimeFilter.h"
#include "sound-processing/sound-handlers/spectrum-stack/UniformBlur.h"
#include "sound-processing/sound-handlers/spectrum-stack/legacy_LogarithmicValueMapper.h"
#include "sound-processing/sound-handlers/spectrum-stack/Spectrogram.h"
#include "sound-processing/sound-handlers/spectrum-stack/SingleValueTransformer.h"

#include "option-parser/OptionMap.h"

using namespace std::string_literals;

using namespace audio_analyzer;

HandlerPatcher* HandlerCacheHelper::getHandler(const istring& name) {
	auto& info = patchersCache[name];

	if (!info.updated) {
		info = parseHandler(name % csView(), std::move(info));
		info.updated = true;
	}

	return info.patcher.get();

}

HandlerCacheHelper::HandlerRawInfo HandlerCacheHelper::parseHandler(sview name, HandlerRawInfo handler) {
	string optionName = L"Handler-"s += name;
	auto descriptionOption = rain.read(optionName);
	if (descriptionOption.empty()) {
		optionName = L"Handler_"s += name;
		descriptionOption = rain.read(optionName);
	}

	auto cl = rain.getLogger().context(L"Handler '{}': ", name);

	if (descriptionOption.empty()) {
		cl.error(L"description is not found", name);
		return { };
	}

	utils::OptionMap optionMap = descriptionOption.asMap(L'|', L' ');
	string rawDescription2;
	readRawDescription2(optionMap.get(L"type").asIString(), optionMap, rawDescription2);

	if (handler.rawDescription == descriptionOption.asString() && handler.rawDescription2 == rawDescription2) {
		return handler;
	}
	anythingChanged = true;

	handler.patcher = createHandlerPatcher(optionMap, cl);
	if (handler.patcher == nullptr) {
		return { };
	}

	const auto unusedOptions = optionMap.getListOfUntouched();
	if (unusedOptionsWarning && !unusedOptions.empty()) {
		cl.warning(L"unused options: '{}'", unusedOptions);
	}

	handler.rawDescription2 = std::move(rawDescription2);
	handler.rawDescription = descriptionOption.asString();

	return handler;
}

std::unique_ptr<HandlerPatcher>
HandlerCacheHelper::createHandlerPatcher(
	const utils::OptionMap& optionMap,
	Logger& cl
) const {
	const auto type = optionMap.get(L"type").asIString();

	if (type.empty()) {
		cl.error(L"type is not found");
		return { };
	}

	if (type == L"rms") {
		return createPatcher<BlockRms>(optionMap, cl);
	}
	if (type == L"peak") {
		return createPatcher<BlockPeak>(optionMap, cl);
	}
	if (type == L"fft") {
		return createPatcher<FftAnalyzer>(optionMap, cl);
	}
	if (type == L"BandResampler") {
		return createPatcher<BandResampler>(optionMap, cl);
	}
	if (type == L"BandCascadeTransformer") {
		return createPatcher<BandCascadeTransformer>(optionMap, cl);
	}
	if (type == L"WeightedBlur") {
		return createPatcher<WeightedBlur>(optionMap, cl);
	}
	if (type == L"UniformBlur") {
		return createPatcher<UniformBlur>(optionMap, cl);
	}
	if (type == L"spectrogram") {
		return createPatcher<Spectrogram>(optionMap, cl);
	}
	if (type == L"waveform") {
		return createPatcher<WaveForm>(optionMap, cl);
	}
	if (type == L"loudness") {
		return createPatcher<Loudness>(optionMap, cl);
	}
	if (type == L"ValueTransformer") {
		return createPatcher<SingleValueTransformer>(optionMap, cl);
	}
	if (type == L"FiniteTimeFilter") {
		return createPatcher<legacy_FiniteTimeFilter>(optionMap, cl);
	}
	if (type == L"LogarithmicValueMapper") {
		return createPatcher<legacy_LogarithmicValueMapper>(optionMap, cl);
	}

	cl.error(L"unknown type '{}'", type);
	return nullptr;
}

void HandlerCacheHelper::readRawDescription2(
	isview type,
	const utils::OptionMap& optionMap,
	string& rawDescription2
) const {
	if (type == L"BandResampler") {
		auto freqListIndex = optionMap.getUntouched(L"freqList").asString();
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
