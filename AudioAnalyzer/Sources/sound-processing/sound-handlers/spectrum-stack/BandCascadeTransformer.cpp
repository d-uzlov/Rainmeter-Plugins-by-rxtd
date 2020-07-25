/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandCascadeTransformer.h"
#include "Math.h"
#include "BandResampler.h"
#include "option-parser/OptionMap.h"

#include "undef.h"
#include "ResamplerProvider.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BandCascadeTransformer::Params>
BandCascadeTransformer::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.minWeight = std::max<double>(
		optionMap.get(L"minWeight"sv).asFloat(0.1),
		std::numeric_limits<float>::epsilon()
	);
	params.targetWeight = std::max<double>(optionMap.get(L"targetWeight"sv).asFloat(2.5), params.minWeight);
	params.weightFallback = std::clamp(optionMap.get(L"weightFallback"sv).asFloat(0.4), 0.0, 1.0) * params.targetWeight;

	params.zeroLevel = std::max<double>(optionMap.get(L"zeroLevelMultiplier"sv).asFloat(1.0), 0.0)
		* 0.66
		* std::numeric_limits<float>::epsilon();
	params.zeroLevelHard = std::clamp<double>(optionMap.get(L"zeroLevelHardMultiplier"sv).asFloat(0.01), 0.0, 1.0)
		* params.zeroLevel;
	params.zeroWeight = std::max<double>(
		optionMap.get(L"zeroWeightMultiplier"sv).asFloat(1.0),
		std::numeric_limits<float>::epsilon()
	);

	if (const auto mixFunctionString = optionMap.get(L"mixFunction"sv).asIString(L"product");
		mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		cl.warning(L"mixFunction '{}' is not recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	return params;
}

void BandCascadeTransformer::setParams(Params _params, Channel channel) {
	if (this->params == _params) {
		return;
	}

	this->params = std::move(_params);

	analysisComputed = false;
	setValid(true);
}

void BandCascadeTransformer::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void BandCascadeTransformer::_finish(const DataSupplier& dataSupplier) {
	if (!changed) {
		return;
	}

	setValid(false);

	const auto source = dataSupplier.getHandler(params.sourceId);
	if (source == nullptr) {
		return;
	}

	const BandResampler* resampler = nullptr;
	resampler = dynamic_cast<const BandResampler*>(source);
	if (resampler == nullptr) {
		const auto provider = dynamic_cast<const ResamplerProvider*>(source);
		if (provider == nullptr) {
			return;
		}
		resampler = provider->getResampler(dataSupplier);
		if (resampler == nullptr) {
			return;
		}
	}

	if (!analysisComputed) {
		computeAnalysis(*resampler, resampler->getStartingLayer(), resampler->getEndCascade());
	}

	if (analysis.minCascadeUsed < 0) {
		return;
	}

	updateValues(*source, *resampler);
	changed = false;

	setValid(true);
}

void BandCascadeTransformer::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;
	analysisComputed = false;
}

bool BandCascadeTransformer::getProp(const isview& prop, utils::BufferPrinter& printer) const {
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
				value = utils::Math::fastPow(value, 1.0 / cascadesSummed);
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
