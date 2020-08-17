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

using namespace audio_analyzer;

SoundHandler::ParseResult BandCascadeTransformer::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	const double epsilon = std::numeric_limits<float>::epsilon();

	params.minWeight = om.get(L"minWeight").asFloat(0.1);
	params.minWeight = std::max(params.minWeight, epsilon);

	params.targetWeight = om.get(L"targetWeight").asFloat(2.5);
	params.targetWeight = std::max(params.targetWeight, params.minWeight);

	params.weightFallback = om.get(L"weightFallback").asFloat(0.4);
	params.weightFallback = std::clamp(params.weightFallback, 0.0, 1.0) * params.targetWeight;

	params.zeroLevel = om.get(L"zeroLevelMultiplier").asFloat(1.0);
	params.zeroLevel = std::max(params.zeroLevel, 0.0);
	params.zeroLevel = params.zeroLevel * 0.66 * epsilon;

	params.zeroLevelHard = om.get(L"zeroLevelHardMultiplier").asFloat(0.01);
	params.zeroLevelHard = std::clamp(params.zeroLevelHard, 0.0, 1.0) * params.zeroLevel;

	params.zeroWeight = om.get(L"zeroWeightMultiplier").asFloat(1.0);
	params.zeroWeight = std::max(params.zeroWeight, epsilon);

	if (const auto mixFunctionString = om.get(L"mixFunction").asIString(L"product");
		mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		cl.warning(L"mixFunction '{}' is not recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	return { params, sourceId % own() };
}

SoundHandler::ConfigurationResult BandCascadeTransformer::vConfigure(Logger& cl) {
	auto& config = getConfiguration();
	const auto provider = dynamic_cast<ResamplerProvider*>(config.sourcePtr);
	if (provider == nullptr) {
		cl.error(L"invalid source: BandResampler is not found in the handler chain");
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

	const auto dataSize = config.sourcePtr->getDataSize();
	snapshot.clear();
	snapshot.resize(dataSize.layersCount);

	return { 1, dataSize.valuesCount };
}

void BandCascadeTransformer::vProcess(array_view<float> wave, clock::time_point killTime) {
	auto& source = *getConfiguration().sourcePtr;

	const index bandsCount = source.getDataSize().valuesCount;
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; i++) {
		auto& meta = snapshot[i];
		meta.nextChunkIndex = 0;
		meta.data = source.getSavedData(i);
	}

	for (auto chunk : source.getChunks(0)) {
		for (index i = 0; i < layersCount; i++) {
			auto& meta = snapshot[i];
			meta.offset -= chunk.equivalentWaveSize;

			auto layerChunks = source.getChunks(i);
			if (meta.nextChunkIndex >= layerChunks.size() || meta.offset >= 0) {
				continue;
			}

			auto nextChunk = layerChunks[meta.nextChunkIndex];
			meta.data = nextChunk.data;
			meta.nextChunkIndex++;
			meta.offset += nextChunk.equivalentWaveSize;
		}

		auto dest = pushLayer(0, chunk.equivalentWaveSize);
		for (index band = 0; band < bandsCount; ++band) {
			dest[band] = computeForBand(band);
		}
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

float BandCascadeTransformer::computeForBand(index band) const {
	const BandResampler& resampler = *resamplerPtr;

	float weight = 0.0;
	index cascadesSummed = 0;
	const index bandEndCascade = analysis.bandEndCascades[band] - analysis.minCascadeUsed;

	float valueProduct = 1.0;
	float valueSum = 0.0;

	const auto bandWeights = resampler.getBandWeights(band);

	for (index cascade = 0; cascade < bandEndCascade; cascade++) {
		const auto bandWeight = bandWeights[cascade];
		const auto magnitude = snapshot[cascade].data[band];
		const auto cascadeBandValue = magnitude / bandWeight;

		if (cascadeBandValue < params.zeroLevelHard) {
			// todo read all of this and try to understand
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
			const auto magnitude = snapshot[cascade].data[band];
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

	valueProduct = std::powf(valueProduct, 1.0f / cascadesSummed);
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
