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

#include "undef.h"

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

void BandCascadeTransformer::setParams(const Params& _params, Channel channel) {
	params = _params;

	analysisComputed = false; // todo compute?
}

void BandCascadeTransformer::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void BandCascadeTransformer::_finish() {
	if (!changed) {
		return;
	}

	resampler->finish();
	source->finish();

	if (!analysisComputed) {
		computeAnalysis(*resampler, resampler->getStartingLayer(), resampler->getEndCascade());
	}

	if (analysis.minCascadeUsed < 0) {
		setValid(false);
		return;
	}

	updateValues(*source, *resampler);
	changed = false;
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

bool BandCascadeTransformer::vCheckSources(Logger& cl) {
	source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	const auto provider = dynamic_cast<ResamplerProvider*>(source);
	if (provider == nullptr) {
		cl.error(L"invalid source");
		return false;
	}

	resampler = provider->getResampler();
	if (resampler == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return false;
	}

	return true;
}

void BandCascadeTransformer::updateValues(SoundHandler& source, BandResampler& resampler) {
	const auto sourceData = source.getData();
	
	const index bandsCount = sourceData[0].values.size();

	resultValues.resize(bandsCount);
	layerData.values = resultValues;
	layerData.id++;

	for (index band = 0; band < bandsCount; ++band) {
		double weight = 0.0;
		index cascadesSummed = 0;
		const index bandEndCascade = analysis.bandEndCascades[band] - analysis.minCascadeUsed;

		double value;
		if (params.mixFunction == MixFunction::PRODUCT) {
			value = 1.0;
		} else {
			value = 0.0;
		}

		for (index cascade = 0; cascade < bandEndCascade; cascade++) {
			const auto bandWeight = resampler.getBandWeights(cascade)[band];
			const auto magnitude = sourceData[cascade].values[band];
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
			for (index cascade = 0; cascade < bandEndCascade; cascade++) {
				const auto bandWeight = resampler.getBandWeights(cascade)[band];
				const auto magnitude = sourceData[cascade].values[band];
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
				value = utils::MyMath::fastPow(value, 1.0 / cascadesSummed);
			} else {
				value /= cascadesSummed;
			}
		} else {
			value = 0.0;
		}

		resultValues[band] = float(value); // TODO float? double?
	}
}

// todo check source data size
void BandCascadeTransformer::computeAnalysis(BandResampler& resampler, index startCascade, index endCascade) {
	if (analysisComputed) {
		return;
	}

	std::wostringstream out;

	out.precision(1);
	out << std::fixed;

	analysis.minCascadeUsed = -1;
	analysis.maxCascadeUsed = -1;

	analysis.weightError = false;

	const index bandsCount = resampler.getData()[0].values.size();
	analysis.bandEndCascades.resize(bandsCount);

	for (index band = 0; band < bandsCount; ++band) {
		double weight = 0.0;
		index bandMinCascade = -1;
		index bandMaxCascade = -1;

		for (index cascade = startCascade; cascade < endCascade; ++cascade) {
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
