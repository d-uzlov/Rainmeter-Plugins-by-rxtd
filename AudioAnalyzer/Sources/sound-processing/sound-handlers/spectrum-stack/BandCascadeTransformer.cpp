/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandCascadeTransformer.h"
#include "BandResampler.h"
#include "option-parser/OptionMap.h"
#include "ResamplerProvider.h"
#include "MyMath.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool BandCascadeTransformer::parseParams(
	const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	const double epsilon = std::numeric_limits<float>::epsilon();

	params.minWeight = optionMap.get(L"minWeight"sv).asFloat(0.1);
	params.minWeight = std::max(params.minWeight, epsilon);

	params.targetWeight = optionMap.get(L"targetWeight"sv).asFloat(2.5);
	params.targetWeight = std::max(params.targetWeight, params.minWeight);

	params.weightFallback = optionMap.get(L"weightFallback"sv).asFloat(0.4);
	params.weightFallback = std::clamp(params.weightFallback, 0.0, 1.0) * params.targetWeight;

	params.zeroLevel = optionMap.get(L"zeroLevelMultiplier"sv).asFloat(1.0);
	params.zeroLevel = std::max(params.zeroLevel, 0.0);
	params.zeroLevel = params.zeroLevel * 0.66 * epsilon;

	params.zeroLevelHard = optionMap.get(L"zeroLevelHardMultiplier"sv).asFloat(0.01);
	params.zeroLevelHard = std::clamp(params.zeroLevelHard, 0.0, 1.0) * params.zeroLevel;

	params.zeroWeight = optionMap.get(L"zeroWeightMultiplier"sv).asFloat(1.0);
	params.zeroWeight = std::max(params.zeroWeight, epsilon);

	if (const auto mixFunctionString = optionMap.get(L"mixFunction"sv).asIString(L"product");
		mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		cl.warning(L"mixFunction '{}' is not recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	return true;
}

SoundHandler::LinkingResult BandCascadeTransformer::vFinishLinking(Logger& cl) {
	const auto sourcePtr = getSource();
	if (sourcePtr == nullptr) {
		cl.error(L"source is not found");
		return { };
	}

	const auto provider = dynamic_cast<ResamplerProvider*>(sourcePtr);
	if (provider == nullptr) {
		cl.error(L"invalid source");
		return { };
	}

	resamplerPtr = provider->getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return { };
	}

	analysis = computeAnalysis(
		*resamplerPtr,
		params.minWeight, params.targetWeight
	);
	if (!analysis.anyCascadeUsed) {
		cl.error(L"no input data could be used because of too strict weight constraints");
		return { };
	}

	savedIds.resize(sourcePtr->getDataSize().layersCount);

	const index bandsCount = sourcePtr->getDataSize().valuesCount;

	return { 1, bandsCount };
}

void BandCascadeTransformer::vReset() {
	changed = true;
}

void BandCascadeTransformer::vProcess(array_view<float> wave) {
	changed = true;
}

void BandCascadeTransformer::vFinish() {
	if (!changed) {
		return;
	}
	changed = false;

	auto& source = *getSource();

	source.finish();
	const auto sourceData = source.getData();

	bool anyChanged = false;
	for (index i = 0; i < sourceData.ids.size(); i++) {
		if (savedIds[i] != sourceData.ids[i]) {
			savedIds[i] = sourceData.ids[i];
			anyChanged = true;
		}
	}
	if (!anyChanged) {
		return;
	}

	const index bandsCount = source.getDataSize().valuesCount;
	auto dest = generateLayerData(0);

	for (index band = 0; band < bandsCount; ++band) {
		dest[band] = computeForBand(band, sourceData.values);
	}
}

bool BandCascadeTransformer::vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
	if (prop == L"cascade analysis") {
		printer.print(analysis.analysisString);
	} else if (prop == L"min cascade used") {
		printer.print(analysis.minCascadeUsed + 1);
	} else if (prop == L"max cascade used") {
		printer.print(analysis.maxCascadeUsed + 1);
	} else {
		return false;
	}

	return true;
}

float BandCascadeTransformer::computeForBand(index band, utils::array2d_view<float> sourceData) const {
	const BandResampler& resampler = *resamplerPtr;

	double weight = 0.0;
	index cascadesSummed = 0;
	const index bandEndCascade = analysis.bandEndCascades[band] - analysis.minCascadeUsed;

	double valueProduct = 1.0;
	double valueSum = 0.0;

	const auto bandWeights = resampler.getBandWeights(band);

	for (index cascade = 0; cascade < bandEndCascade; cascade++) {
		const auto bandWeight = bandWeights[cascade];
		const auto magnitude = sourceData[cascade][band];
		const auto cascadeBandValue = magnitude / bandWeight;

		if (cascadeBandValue < params.zeroLevelHard) { // todo read all of this and try to understand
			break;
		}
		if (bandWeight < params.minWeight) {
			continue;
		}

		valueProduct *= cascadeBandValue;
		valueSum += cascadeBandValue;

		weight += bandWeight;
		cascadesSummed++;

		if (cascadeBandValue < params.zeroLevel) {
			break;
		}
	}

	if (weight < params.weightFallback) {
		for (index cascade = 0; cascade < bandEndCascade; cascade++) {
			const auto bandWeight = bandWeights[cascade];
			const auto magnitude = sourceData[cascade][band];
			const auto cascadeBandValue = magnitude / bandWeight;

			if (cascadeBandValue < params.zeroLevelHard) {
				break;
			}
			if (bandWeight < params.zeroWeight) {
				continue;
			}
			if (bandWeight >= params.minWeight) {
				continue;
			}

			valueProduct *= cascadeBandValue;
			valueSum += cascadeBandValue;

			weight += bandWeight;
			cascadesSummed++;

			if (cascadeBandValue < params.zeroLevel) {
				break;
			}
			if (weight >= params.weightFallback) {
				break;
			}
		}
	}

	if (cascadesSummed <= 0) {
		return 0.0f;
	}

	valueProduct = utils::MyMath::fastPow(valueProduct, 1.0 / cascadesSummed);
	valueSum /= cascadesSummed;

	return float(params.mixFunction == MixFunction::PRODUCT ? valueProduct : valueSum);
}

BandCascadeTransformer::AnalysisInfo
BandCascadeTransformer::computeAnalysis(BandResampler& resampler, double minWeight, double targetWeight) {
	const index startCascade = resampler.getStartingLayer();
	const index endCascade = resampler.getEndCascade();

	std::wostringstream out;

	out.precision(1);
	out << std::fixed;

	AnalysisInfo analysis{ };
	analysis.minCascadeUsed = std::numeric_limits<index>::max();
	analysis.maxCascadeUsed = std::numeric_limits<index>::min();

	const index bandsCount = resampler.getDataSize().valuesCount;
	analysis.bandEndCascades.resize(bandsCount);

	for (index band = 0; band < bandsCount; ++band) {
		double weight = 0.0;
		index bandMinCascade = -1;
		index bandMaxCascade = -1;
		bool anyCascadeUsed = false;

		const auto bandWeights = resampler.getBandWeights(band);

		for (index cascade = startCascade; cascade < endCascade; ++cascade) {
			const auto bandWeight = bandWeights[cascade - startCascade];

			if (bandWeight < minWeight) {
				continue;
			}

			anyCascadeUsed = true;
			weight += bandWeight;
			if (bandMinCascade < 0) {
				bandMinCascade = cascade;
			}
			bandMaxCascade = cascade;

			if (weight >= targetWeight) {
				break;
			}
		}


		out << band;
		out << L":";

		if (!anyCascadeUsed) {
			out << L"!!";
			bandMinCascade = startCascade;
			bandMaxCascade = endCascade - 1;
		} else {
			analysis.minCascadeUsed = std::min(analysis.minCascadeUsed, bandMinCascade);
			analysis.maxCascadeUsed = std::max(analysis.maxCascadeUsed, bandMaxCascade);
			analysis.anyCascadeUsed = true;
		}

		analysis.bandEndCascades[band] = bandMaxCascade + 1;

		out << weight;
		out << L":";
		out << bandMinCascade + 1;
		out << L"-";
		out << bandMaxCascade + 1;
		out << L" ";

	}

	analysis.analysisString = out.str();

	return analysis;
}
