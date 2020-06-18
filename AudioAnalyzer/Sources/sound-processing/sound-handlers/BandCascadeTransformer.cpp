/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandCascadeTransformer.h"
#include "FastMath.h"
#include "BandResampler.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BandCascadeTransformer::Params> BandCascadeTransformer::parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0.1), std::numeric_limits<float>::epsilon());
	params.targetWeight = std::max<double>(optionMap.get(L"targetWeight"sv).asFloat(2.5), params.minWeight);
	params.weightFallback = std::clamp(optionMap.get(L"weightFallback"sv).asFloat(0.4), 0.0, 1.0) * params.targetWeight;

	params.zeroLevel = std::max<double>(optionMap.get(L"zeroLevelMultiplier"sv).asFloat(1.0), 0.0) * 0.66
		* std::numeric_limits<float>::epsilon();
	params.zeroLevelHard = std::clamp<double>(optionMap.get(L"zeroLevelHardMultiplier"sv).asFloat(0.01), 0.0, 1.0)
		* params.zeroLevel;
	params.zeroWeight = std::max<double>(optionMap.get(L"zeroWeightMultiplier"sv).asFloat(1.0), std::numeric_limits<float>::epsilon());

	if (auto mixFunctionString = optionMap.get(L"mixFunction"sv).asIString(L"product");
		mixFunctionString.empty() || mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		cl.warning(L"mixFunction '{}' now recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	return params;
}

void BandCascadeTransformer::setParams(Params _params) {
	this->params = std::move(_params);

	analysisComputed = false;
	valid = true;
}

void BandCascadeTransformer::process(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	changed = true;
}

void BandCascadeTransformer::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void BandCascadeTransformer::finish(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	if (!changed) {
		return;
	}

	valid = false;

	const auto source = dataSupplier.getHandler<ResamplerProvider>(params.sourceId);
	if (source == nullptr) {
		return;
	}

	const auto resampler = source->getResampler();
	if (resampler == nullptr) {
		return;
	}

	if (!analysisComputed) {
		computeAnalysis(*resampler, resampler->getStartingLayer(), resampler->getEndCascade());
	}

	if (analysis.minCascadeUsed < 0) {
		return;
	}

	updateValues(*source, *resampler);
	changed = false;

	valid = true;
}

void BandCascadeTransformer::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
	analysisComputed = false;
}

const wchar_t* BandCascadeTransformer::getProp(const isview& prop) const {
	propString.clear();

	if (prop == L"cascade analysis") {
		return analysis.analysisString.c_str();
	} else if (prop == L"min cascade used") {
		propString = std::to_wstring(analysis.minCascadeUsed + 1);
	} else if (prop == L"max cascade used") {
		propString = std::to_wstring(analysis.maxCascadeUsed + 1);
	} else {
		return nullptr;
	}

	return propString.c_str();
}

void BandCascadeTransformer::reset() {
	changed = true;
}

void BandCascadeTransformer::updateValues(const SoundHandler& source, const BandResampler& resampler) {
	
	const index bandsCount = resampler.getData(0).size();

	resultValues.resize(bandsCount);

	for (index band = 0; band < bandsCount; ++band) {
		double weight = 0.0;
		layer_t cascadesSummed = 0;
		const layer_t bandEndCascade = analysis.bandEndCascades[band] - analysis.minCascadeUsed;

		double value;
		if (params.mixFunction == MixFunction::PRODUCT) {
			value = 1.0;
		} else {
			value = 0.0;
		}

		for (layer_t cascade = 0; cascade < bandEndCascade; cascade++) {
			const auto bandWeight = resampler.getBandWeights(cascade)[band];
			const auto magnitude = source.getData(cascade)[band];
			const auto cascadeBandValue = magnitude / bandWeight;

			if (cascadeBandValue < params.zeroLevelHard) {
				break;
			}
			if (bandWeight < params.minWeight) {
				continue;
			}

			if (params.mixFunction == MixFunction::PRODUCT) {
				value *= cascadeBandValue;
			} else {
				value += cascadeBandValue;
			}

			weight += bandWeight;
			cascadesSummed++;

			if (cascadeBandValue < params.zeroLevel) {
				break;
			}
		}

		if (weight < params.weightFallback) {
			for (layer_t cascade = 0; cascade < bandEndCascade; cascade++) {
				const auto bandWeight = resampler.getBandWeights(cascade)[band];
				const auto magnitude = source.getData(cascade)[band];
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


				if (params.mixFunction == MixFunction::PRODUCT) {
					value *= cascadeBandValue;
				} else {
					value += cascadeBandValue;
				}

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

		if (cascadesSummed > 0) {
			if (params.mixFunction == MixFunction::PRODUCT) {
				value = utils::FastMath::pow(value, 1.0 / cascadesSummed);
			} else {
				value /= cascadesSummed;
			}
		} else {
			value = 0.0;
		}

		resultValues[band] = value;
	}
}

void BandCascadeTransformer::computeAnalysis(const BandResampler& resampler, layer_t startCascade, layer_t endCascade) {
	if (analysisComputed) {
		return;
	}

	std::wostringstream out;

	out.precision(1);
	out << std::fixed;

	analysis.minCascadeUsed = -1;
	analysis.maxCascadeUsed = -1;

	analysis.weightError = false;

	const index bandsCount = resampler.getData(0).size();
	analysis.bandEndCascades.resize(bandsCount);

	for (index band = 0; band < bandsCount; ++band) {
		double weight = 0.0;
		layer_t bandMinCascade = -1;
		layer_t bandMaxCascade = -1;

		for (layer_t cascade = startCascade; cascade < endCascade; ++cascade) {
			const auto bandWeight = resampler.getBandWeights(cascade - startCascade)[band];

			if (bandWeight >= params.minWeight) {
				weight += bandWeight;
				if (bandMinCascade < 0) {
					bandMinCascade = cascade;
				}
				bandMaxCascade = cascade;
			}

			if (weight >= params.targetWeight) {
				break;
			}
		}


		out << band;
		out << L":";

		if (bandMinCascade < 0) {
			out << L"!!";
			bandMinCascade = startCascade;
			bandMaxCascade = endCascade - 1;
			analysis.weightError = true;
		}

		if (analysis.minCascadeUsed < 0) {
			analysis.minCascadeUsed = bandMinCascade;
		} else {
			analysis.minCascadeUsed = std::min(analysis.minCascadeUsed, bandMinCascade);
		}
		if (analysis.maxCascadeUsed < 0) {
			analysis.maxCascadeUsed = bandMaxCascade;
		} else {
			analysis.maxCascadeUsed = std::max(analysis.maxCascadeUsed, bandMaxCascade);
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

	// const layer_t minVectorIndex = analysis.minCascadeUsed - startCascade;
	// const layer_t maxVectorIndex = analysis.maxCascadeUsed - startCascade;
	//
	// if (maxVectorIndex + 1 < layer_t(cascadesInfo.size())) {
	// 	cascadesInfo.erase(cascadesInfo.begin() + (maxVectorIndex + 1), cascadesInfo.end());
	// }
	// if (minVectorIndex > 0) {
	// 	cascadesInfo.erase(cascadesInfo.begin(), cascadesInfo.begin() + (minVectorIndex - 1));
	// }

	analysisComputed = true;
}
