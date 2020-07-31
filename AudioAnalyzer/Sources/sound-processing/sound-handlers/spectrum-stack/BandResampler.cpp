/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandResampler.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionList.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BandResampler::Params>
BandResampler::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain) {
	Params params;
	params.fftId = optionMap.get(L"source"sv).asIString();
	if (params.fftId.empty()) {
		cl.error(L"source is not found");
		return std::nullopt;
	}

	auto freqListIndex = optionMap.get(L"freqList"sv).asString();
	if (freqListIndex.empty()) {
		cl.error(L"freqList is not found");
		return std::nullopt;
	}

	auto freqListOptionName = L"FreqList-"s += freqListIndex;
	auto freqListOption = rain.read(freqListOptionName);
	if (freqListOption.empty()) {
		freqListOptionName = L"FreqList_"s += freqListIndex;
		freqListOption = rain.read(freqListOptionName);
	}

	const auto bounds = freqListOption.asList(L'|');
	utils::Rainmeter::Logger freqListLogger = rain.getLogger().context(L"{}: ", freqListOptionName);
	auto freqsOpt = parseFreqList(bounds, freqListLogger, rain);
	if (!freqsOpt.has_value()) {
		cl.error(L"freqList '{}' can't be parsed", freqListIndex);
		return std::nullopt;
	}
	params.bandFreqs = freqsOpt.value();

	params.minCascade = std::max(optionMap.get(L"minCascade"sv).asInt(0), 0);
	params.maxCascade = std::max(optionMap.get(L"maxCascade"sv).asInt(0), 0);

	params.includeDC = optionMap.get(L"includeDC"sv).asBool(true);

	params.legacy_proportionalValues = optionMap.get(L"proportionalValues"sv).asBool(true);

	return params;
}

void BandResampler::setParams(const Params& _params, Channel channel) {
	if (params == _params) {
		return;
	}

	params = _params;

	bandsCount = index(params.bandFreqs.size() - 1);

	if (bandsCount < 1) {
		setValid(false);
		return;
	}

	if (params.legacy_proportionalValues) {
		legacy_generateBandMultipliers();
	}

	cascadeInfoIsCalculated = false;
}

void BandResampler::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void BandResampler::_finish() {
	if (changed) {
		updateValues();
		changed = false;
	}
}

array_view<float> BandResampler::getData(index layer) const {
	return cascadesInfo[layer].magnitudes;
}

index BandResampler::getLayersCount() const {
	return index(cascadesInfo.size());
}

void BandResampler::setSamplesPerSec(index value) {
	samplesPerSec = value;
	cascadeInfoIsCalculated = false;
}

bool BandResampler::getProp(const isview& prop, utils::BufferPrinter& printer) const {
	if (prop == L"bands count") {
		printer.print(bandsCount);
	} else {
		auto index = legacy_parseIndexProp(prop, L"lower bound", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			printer.print(params.bandFreqs[index]);
			return true;
		}

		index = legacy_parseIndexProp(prop, L"upper bound", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			printer.print(params.bandFreqs[index + 1]);
			return true;
		}

		index = legacy_parseIndexProp(prop, L"central frequency", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			printer.print((params.bandFreqs[index] + params.bandFreqs[index + 1]) * 0.5);
			return true;
		}

		return false;
	}

	return true;
}

void BandResampler::reset() {
	// no 
	changed = true;
}


array_view<float> BandResampler::getBandWeights(index cascade) const {
	return cascadesInfo[cascade].weights;
}

array_view<float> BandResampler::getBaseFreqs() const {
	return params.bandFreqs;
}

bool BandResampler::vCheckSources(Logger& cl) {
	// todo what if user want to filter values between fft and resampler?
	// todo use getSource everywhere instead of saving source field

	const auto source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	this->source = dynamic_cast<const FftAnalyzer*>(source);
	if (this->source == nullptr) {
		cl.error(L"invalid source, need FftAnalyzer");
		return false;
	}

	return true;
}

void BandResampler::updateValues() {
	if (!cascadeInfoIsCalculated) {
		const auto cascadesCount = source->getLayersCount();
		computeCascadesInfo(source->getFftSize(), cascadesCount);
		cascadeInfoIsCalculated = true;
	}

	sampleData(*source);
}

void BandResampler::sampleData(const FftAnalyzer& source) {
	const auto fftBinsCount = source.getData(0).size();
	double binWidth = static_cast<double>(samplesPerSec) / (source.getFftSize() * std::pow(2, startCascade));

	for (auto cascade = startCascade; cascade < endCascade; ++cascade) {

		const auto fftData = source.getData(cascade);

		auto& cascadeMagnitudes = cascadesInfo[cascade - startCascade].magnitudes;
		std::fill(cascadeMagnitudes.begin(), cascadeMagnitudes.end(), 0.0f);

		sampleCascade(fftData, cascadeMagnitudes, binWidth, fftBinsCount);

		binWidth *= 0.5;
	}

	// legacy
	if (params.legacy_proportionalValues) {
		for (auto& [cascadeMagnitudes, cascadeWeights] : cascadesInfo) {
			for (index band = 0; band < bandsCount; ++band) {
				cascadeMagnitudes[band] *= bandFreqMultipliers[band];
			}
		}
	}
}

void BandResampler::sampleCascade(
	array_view<float> fftData,
	array_span<float> result,
	double binWidth,
	index fftBinsCount
) {
	const double binWidthInverse = 1.0 / binWidth;

	index bin = params.includeDC ? 0 : 1; // bin 0 is DC
	index band = 0;

	double bandMinFreq = params.bandFreqs[0];
	double bandMaxFreq = params.bandFreqs[1];

	double value = 0.0;

	while (bin < fftBinsCount && band < bandsCount) {
		const double binUpperFreq = (bin + 0.5) * binWidth;
		if (binUpperFreq < bandMinFreq) {
			bin++;
			continue;
		}

		double weight = 1.0;
		const double binLowerFreq = (bin - 0.5) * binWidth;

		if (binLowerFreq < bandMinFreq) {
			weight -= (bandMinFreq - binLowerFreq) * binWidthInverse;
		}
		if (binUpperFreq > bandMaxFreq) {
			weight -= (binUpperFreq - bandMaxFreq) * binWidthInverse;
		}
		if (weight > 0) {
			const auto fftValue = fftData[bin];
			value += fftValue * weight;
		}

		if (bandMaxFreq >= binUpperFreq) {
			bin++;
		} else {
			result[band] = float(value);
			value = 0.0;
			band++;

			if (band >= bandsCount) {
				break;
			}

			bandMinFreq = bandMaxFreq;
			bandMaxFreq = params.bandFreqs[band + 1];
		}
	}
}

std::optional<std::vector<float>>
BandResampler::parseFreqList(const utils::OptionList& bounds, Logger& cl, const Rainmeter& rain) {
	std::vector<float> freqs;

	for (auto boundOption : bounds) {
		auto options = boundOption.asList(L' ');
		auto type = options.get(0).asIString();

		if (type == L"linear" || type == L"log") {

			if (options.size() != 4) {
				cl.error(L"{} must have 3 options (count, min, max)", type);
				return std::nullopt;
			}

			const index count = options.get(1).asInt(0);
			if (count < 1) {
				cl.error(L"count must be >= 1");
				return std::nullopt;
			}

			const auto min = options.get(2).asFloatF();
			const auto max = options.get(3).asFloatF();
			if (min >= max) {
				cl.error(L"min must be < max");
				return std::nullopt;
			}

			if (type == L"linear") {
				const auto delta = max - min;

				for (index i = 0; i <= count; ++i) {
					freqs.push_back(min + delta * i / count);
				}
			} else {
				// log
				const auto step = std::pow(2.0f, std::log2(max / min) / count);
				auto freq = min;
				freqs.push_back(freq);

				for (index i = 0; i < count; ++i) {
					freq *= step;
					freqs.push_back(freq);
				}
			}
			continue;
		}
		if (type == L"custom") {
			if (options.size() < 2) {
				cl.error(L"custom must have at least two frequencies specified but {} found", options.size());
				return std::nullopt;
			}
			for (index i = 1; i < options.size(); ++i) {
				freqs.push_back(options.get(i).asFloatF());
			}
			continue;
		}

		cl.error(L"unknown list type '{}'", type);
		return std::nullopt;
	}

	std::sort(freqs.begin(), freqs.end());

	const double threshold = rain.readDouble(L"FreqSimThreshold", 0.07);
	// 0.07 is a random constant that I feel appropriate

	std::vector<float> result;
	result.reserve(freqs.size());
	float lastValue = -1;
	for (auto value : freqs) {
		if (value <= 0) {
			cl.error(L"frequency must be > 0 ({} found)", value);
			return std::nullopt;
		}
		if (value - lastValue < threshold) {
			continue;
		}
		result.push_back(value);
		lastValue = value;
	}

	if (result.size() < 2) {
		cl.error(L"need >= 2 frequencies but only {} found", result.size());
		return std::nullopt;
	}

	return result;
}

void BandResampler::computeCascadesInfo(index fftSize, index cascadesCount) {
	startCascade = 1;
	endCascade = cascadesCount + 1;
	if (params.minCascade > 0) {
		if (cascadesCount < params.minCascade) {
			return;
		}

		startCascade = params.minCascade;

		if (params.maxCascade >= params.minCascade && cascadesCount >= params.maxCascade) {
			endCascade = params.maxCascade + 1;
		}
	}
	startCascade--;
	endCascade--;

	cascadesInfo.resize(endCascade - startCascade);
	for (auto& cascadeInfo : cascadesInfo) {
		cascadeInfo.setSize(bandsCount);
	}

	const auto fftBinsCount = fftSize / 2;
	double binWidth = static_cast<double>(samplesPerSec) / (fftSize * std::pow(2, startCascade));

	for (auto& [_, cascadeWeights] : cascadesInfo) {
		calculateCascadeWeights(cascadeWeights, fftBinsCount, binWidth);
		binWidth *= 0.5;
	}
}

void BandResampler::calculateCascadeWeights(array_span<float> result, index fftBinsCount, double binWidth) {
	const double binWidthInverse = 1.0 / binWidth;

	index bin = params.includeDC ? 0 : 1; // bin 0 is ~DC
	index band = 0;

	double bandMinFreq = params.bandFreqs[0];
	double bandMaxFreq = params.bandFreqs[1];

	double bandWeight = 0.0;

	while (bin < fftBinsCount && band < bandsCount) {
		const double binUpperFreq = (bin + 0.5) * binWidth;
		if (binUpperFreq < bandMinFreq) {
			bin++;
			continue;
		}

		double binWeight = 1.0;
		const double binLowerFreq = (bin - 0.5) * binWidth;

		if (binLowerFreq < bandMinFreq) {
			binWeight -= (bandMinFreq - binLowerFreq) * binWidthInverse;
		}
		if (binUpperFreq > bandMaxFreq) {
			binWeight -= (binUpperFreq - bandMaxFreq) * binWidthInverse;
		}
		if (binWeight > 0) {
			bandWeight += binWeight;
		}

		if (bandMaxFreq >= binUpperFreq) {
			bin++;
		} else {
			result[band] = float(bandWeight);
			bandWeight = 0.0;
			band++;
			if (band >= bandsCount) {
				break;
			}
			bandMinFreq = bandMaxFreq;
			bandMaxFreq = params.bandFreqs[band + 1];
		}
	}
}

void BandResampler::legacy_generateBandMultipliers() {
	bandFreqMultipliers.resize(bandsCount);
	double multipliersSum{ };
	for (index i = 0; i < bandsCount; ++i) {
		bandFreqMultipliers[i] = std::log(params.bandFreqs[i + 1] - params.bandFreqs[i] + 1.0f);
		// bandFreqMultipliers[i] = params.bandFreqs[i + 1] - params.bandFreqs[i];
		multipliersSum += bandFreqMultipliers[i];
	}
	const double bandFreqMultipliersAverage = multipliersSum / bandsCount;
	const double multiplierCorrectingConstant = 1.0 / bandFreqMultipliersAverage;
	for (auto& multiplier : bandFreqMultipliers) {
		multiplier = float(multiplier * multiplierCorrectingConstant);
	}
}
