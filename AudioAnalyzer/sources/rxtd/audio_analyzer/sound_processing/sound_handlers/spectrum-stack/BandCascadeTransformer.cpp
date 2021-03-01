/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandCascadeTransformer.h"
#include "BandResampler.h"
#include "ResamplerProvider.h"

using rxtd::audio_analyzer::handler::BandCascadeTransformer;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer BandCascadeTransformer::vParseParams(ParamParseContext& context) const noexcept(false) {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	const auto sourceId = context.options.get(L"source").asIString();
	if (sourceId.empty()) {
		context.log.error(L"source not found");
		throw InvalidOptionsException{};
	}

	const float epsilon = std::numeric_limits<float>::epsilon();

	params.minWeight = context.options.get(L"minWeight").asFloatF(0.1f);
	params.minWeight = std::max(params.minWeight, epsilon);

	params.targetWeight = context.options.get(L"targetWeight").asFloatF(2.5f);
	params.targetWeight = std::max(params.targetWeight, params.minWeight);

	params.zeroLevelHard = context.options.get(L"zeroLevelMultiplier").asFloatF(1.0f);
	params.zeroLevelHard = std::max(params.zeroLevelHard, 0.0f);
	params.zeroLevelHard *= epsilon;

	if (const auto mixFunctionString = context.options.get(L"mixFunction").asIString(L"product");
		mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		context.log.warning(L"mixFunction '{}' is not recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	return result;
}

HandlerBase::ConfigurationResult
BandCascadeTransformer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const auto provider = dynamic_cast<ResamplerProvider*>(config.sourcePtr);
	if (provider == nullptr) {
		cl.error(L"invalid source: BandResampler is not found in the handler chain");
		return {};
	}

	resamplerPtr = provider->getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return {};
	}

	const auto dataSize = config.sourcePtr->getDataSize();
	snapshot.clear();
	snapshot.resize(static_cast<size_t>(dataSize.layersCount));

	return { dataSize.valuesCount, { config.sourcePtr->getDataSize().eqWaveSizes[0] } };
}

void BandCascadeTransformer::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& source = *getConfiguration().sourcePtr;

	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; i++) {
		auto& meta = snapshot[static_cast<size_t>(i)];
		meta.nextChunkIndex = 0;
		meta.data = source.getSavedData(i);
	}

	auto& eqWS = source.getDataSize().eqWaveSizes;

	for (auto chunk : source.getChunks(0)) {
		auto dest = pushLayer(0);

		if (clock::now() > context.killTime) {
			auto myChunks = getChunks(0);
			myChunks.remove_suffix(1);
			dest.copyFrom(myChunks.empty() ? getSavedData(0) : myChunks.back());
			continue;
		}

		for (index i = 0; i < layersCount; i++) {
			auto& meta = snapshot[static_cast<size_t>(i)];
			meta.offset -= eqWS[0];

			auto layerChunks = source.getChunks(i);
			if (meta.nextChunkIndex >= layerChunks.size() || meta.offset >= 0) {
				continue;
			}

			meta.data = layerChunks[meta.nextChunkIndex];
			meta.nextChunkIndex++;
			meta.offset += eqWS[static_cast<size_t>(i)];
			meta.maxValue = *std::max_element(meta.data.begin(), meta.data.end());
		}

		for (index band = 0; band < dest.size(); ++band) {
			dest[band] = computeForBand(band);
		}
	}
}

float BandCascadeTransformer::computeForBand(index band) const {
	const BandResampler& resampler = *resamplerPtr;

	float weight = 0.0f;
	float cascadesSummed = 0.0f;

	float valueProduct = 1.0f;
	float valueSum = 0.0f;

	const auto bandWeights = resampler.getBandWeights(band);

	for (index cascade = 0; cascade < static_cast<index>(snapshot.size()); cascade++) {
		const float bandWeight = bandWeights[cascade];
		const float magnitude = snapshot[static_cast<size_t>(cascade)].data[band];

		if (snapshot[static_cast<size_t>(cascade)].maxValue <= params.zeroLevelHard) {
			// this most often happens when there was a silence, and then some sound,
			// so cascade with index more than N haven't updated yet
			// so in that case we ignore values of all these cascades

			if (cascadesSummed == 0.0f) {
				// if cascadesSummed == 0.0f then bandWeight < params.minWeight for all previous cascades
				// let's use value of previous cascade to provide at least something
				return cascade == 0 ? 0.0f : snapshot[static_cast<size_t>(cascade - 1)].data[band];
			}
			break;
		}

		if (bandWeight < params.minWeight) {
			continue;
		}

		cascadesSummed += 1.0f;

		valueProduct *= magnitude;
		valueSum += magnitude;
		weight += bandWeight;

		if (weight >= params.targetWeight) {
			break;
		}
	}

	if (cascadesSummed == 0.0f) {
		// bandWeight < params.minWeight for all cascades
		// let's use value of the last cascade with non zero weight and value
		for (index i = bandWeights.size() - 1; i >= 0; i--) {
			if (bandWeights[i] > 0.0f && snapshot[static_cast<size_t>(i)].data[band] > static_cast<float>(params.zeroLevelHard)) {
				return snapshot[static_cast<size_t>(i)].data[band];
			}
		}
		return 0.0f;
	}

	return params.mixFunction == MixFunction::PRODUCT
	       ? std::pow(valueProduct, 1.0f / cascadesSummed)
	       : valueSum / cascadesSummed;
}
