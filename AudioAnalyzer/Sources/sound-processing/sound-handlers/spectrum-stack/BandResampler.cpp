/*
 * Copyright (C) 2019 rxtd
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

std::optional<BandResampler::Params> BandResampler::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl,
	utils::Rainmeter& rain
) {
	Params params;
	params.fftId = optionMap.get(L"source"sv).asIString();
	if (params.fftId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	auto freqListIndex = optionMap.get(L"freqList"sv).asString();
	if (freqListIndex.empty()) {
		cl.error(L"freqList not found");
		return std::nullopt;
	}

	auto freqListOptionName = L"FreqList-"s += freqListIndex;
	auto freqList = rain.read(freqListOptionName);
	if (freqList.empty()) {
		freqListOptionName = L"FreqList_"s += freqListIndex;
		freqList = rain.read(freqListOptionName);
	}

	const auto bounds = freqList.asList(L'|');
	utils::Rainmeter::Logger freqListLogger = rain.getLogger().context(L"{}: ", freqListOptionName);
	auto freqsOpt = parseFreqList(bounds, freqListLogger, rain);
	if (!freqsOpt.has_value()) {
		cl.error(L"freqList '{}' can't be parsed", freqListIndex);
		return std::nullopt;
	}
	params.bandFreqs = freqsOpt.value();
	if (params.bandFreqs.size() < 2) {
		cl.error(L"freqList '{}' must have >= 2 frequencies but only {} found", freqListIndex, params.bandFreqs.size());
		return std::nullopt;
	}

	params.minCascade = std::max<layer_t>(optionMap.get(L"minCascade"sv).asInt<layer_t>(0), 0);
	params.maxCascade = std::max<layer_t>(optionMap.get(L"maxCascade"sv).asInt<layer_t>(0), 0);

	params.proportionalValues = optionMap.get(L"proportionalValues"sv).asBool(true);
	params.includeDC = optionMap.get(L"includeDC"sv).asBool(true);

	return params;
}

void BandResampler::setParams(Params _params, Channel channel) {
	if (this->params == _params) {
		return;
	}

	this->params = std::move(_params);

	valid = false;

	bandsCount = index(params.bandFreqs.size() - 1);

	if (bandsCount < 1) {
		return;
	}

	bandFreqMultipliers.resize(bandsCount);
	double multipliersSum{ };
	for (index i = 0; i < bandsCount; ++i) {
		bandFreqMultipliers[i] = std::log(params.bandFreqs[i + 1] - params.bandFreqs[i] + 1.0);
		// bandFreqMultipliers[i] = params.bandFreqs[i + 1] - params.bandFreqs[i];
		multipliersSum += bandFreqMultipliers[i];
	}
	const double bandFreqMultipliersAverage = multipliersSum / bandsCount;
	const double multiplierCorrectingConstant = 1.0 / bandFreqMultipliersAverage;
	for (double& multiplier : bandFreqMultipliers) {
		multiplier *= multiplierCorrectingConstant;
	}

	valid = true;
}

void BandResampler::process(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	changed = true;
}

void BandResampler::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void BandResampler::finish(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	if (changed) {
		updateValues(dataSupplier);
		changed = false;
	}
}

array_view<float> BandResampler::getData(layer_t layer) const {
	return cascadesInfo[layer].magnitudes;
}

SoundHandler::layer_t BandResampler::getLayersCount() const {
	return layer_t(cascadesInfo.size());
}

void BandResampler::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

const wchar_t* BandResampler::getProp(const isview& prop) const {
	propString.clear();

	if (prop == L"bands count") {
		propString = std::to_wstring(bandsCount);
	} else {
		auto index = parseIndexProp(prop, L"lower bound", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			propString = std::to_wstring(params.bandFreqs[index]);
			return propString.c_str();
		}

		index = parseIndexProp(prop, L"upper bound", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			propString = std::to_wstring(params.bandFreqs[index + 1]);
			return propString.c_str();
		}

		index = parseIndexProp(prop, L"central frequency", bandsCount + 1);
		if (index == -2) {
			return L"0";
		}
		if (index >= 0) {
			if (index > 0) {
				index--;
			}
			propString = std::to_wstring((params.bandFreqs[index] + params.bandFreqs[index + 1]) * 0.5);
			return propString.c_str();
		}

		return nullptr;
	}

	return propString.c_str();
}

void BandResampler::reset() {
	// no 
	changed = true;
}


array_view<float> BandResampler::getBandWeights(layer_t cascade) const {
	return cascadesInfo[cascade].weights;
}

array_view<double> BandResampler::getBaseFreqs() const {
	return params.bandFreqs;
}

void BandResampler::updateValues(const DataSupplier& dataSupplier) {
	valid = false;

	const auto source = dataSupplier.getHandler<FftAnalyzer>(params.fftId);
	if (source == nullptr) {
		return;
	}

	const auto cascadesCount = source->getLayersCount();

	beginCascade = 1;
	endCascade = cascadesCount + 1;
	if (params.minCascade > 0) {
		if (cascadesCount < params.minCascade) {
			return;
		}

		beginCascade = params.minCascade;

		if (params.maxCascade >= params.minCascade && cascadesCount >= params.maxCascade) {
			endCascade = params.maxCascade + 1;
		}
	}
	beginCascade--;
	endCascade--;

	// TODO only do this on reload?
	computeBandInfo(*source, beginCascade, endCascade);

	sampleData(*source, beginCascade, endCascade);

	valid = true;
}

void BandResampler::computeBandInfo(const FftAnalyzer& source, layer_t startCascade, layer_t endCascade) {
	cascadesInfo.resize(endCascade - startCascade);
	for (auto& cascadeInfo : cascadesInfo) {
		cascadeInfo.setSize(bandsCount);
	}

	const auto fftBinsCount = source.getData(0).size();
	double binWidth = static_cast<double>(samplesPerSec) / (source.getFftSize() * std::pow(2, startCascade));
	double binWidthInverse = 1.0 / binWidth;

	for (auto& [_, cascadeWeights] : cascadesInfo) {
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
				cascadeWeights[band] = bandWeight;
				bandWeight = 0.0;
				band++;
				if (band >= bandsCount) {
					break;
				}
				bandMinFreq = bandMaxFreq;
				bandMaxFreq = params.bandFreqs[band + 1];
			}
		}
		binWidth *= 0.5;
		binWidthInverse *= 2.0;
	}
}

void BandResampler::sampleData(const FftAnalyzer& source, layer_t startCascade, layer_t endCascade) {

	const auto fftBinsCount = source.getData(0).size();
	// TODO this should be mostly const between params changes, but can actually change when sampleRate changes
	// TODO change of binWidth invalidates almost everything
	double binWidth = static_cast<double>(samplesPerSec) / (source.getFftSize() * std::pow(2, startCascade));
	double binWidthInverse = 1.0 / binWidth;

	for (auto cascade = startCascade; cascade < endCascade; ++cascade) {
		index bin = params.includeDC ? 0 : 1; // bin 0 is ~DC
		index band = 0;

		double bandMinFreq = params.bandFreqs[0];
		double bandMaxFreq = params.bandFreqs[1];

		const auto fftData = source.getData(cascade);

		auto& cascadeMagnitudes = cascadesInfo[cascade - startCascade].magnitudes;
		std::fill(cascadeMagnitudes.begin(), cascadeMagnitudes.end(), 0.0f);
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
				cascadeMagnitudes[band] = value;
				value = 0.0;
				band++;

				if (band >= bandsCount) {
					break;
				}

				bandMinFreq = bandMaxFreq;
				bandMaxFreq = params.bandFreqs[band + 1];
			}
		}
		binWidth *= 0.5;
		binWidthInverse *= 2.0;
	}

	if (params.proportionalValues) {
		for (auto& [cascadeMagnitudes, cascadeWeights] : cascadesInfo) {
			for (index band = 0; band < bandsCount; ++band) {
				cascadeMagnitudes[band] *= bandFreqMultipliers[band];
			}
		}
	}

}

std::optional<std::vector<double>> BandResampler::parseFreqList(const utils::OptionList& bounds,
                                                                utils::Rainmeter::Logger& cl,
                                                                const utils::Rainmeter& rain) {
	std::vector<double> freqs;

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

			const double min = options.get(2).asFloat();
			const double max = options.get(3).asFloat();
			if (min >= max) {
				cl.error(L"min must be < max");
				return std::nullopt;
			}

			if (type == L"linear") {
				const double delta = max - min;

				for (index i = 0; i <= count; ++i) {
					freqs.push_back(min + delta * i / count);
				}
			} else {
				// log
				const auto step = std::pow(2.0, std::log2(max / min) / count);
				double freq = min;
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
				freqs.push_back(options.get(i).asFloat());
			}
			continue;
		}

		cl.error(L"unknown list type '{}'", type);
		return std::nullopt;
	}

	std::sort(freqs.begin(), freqs.end());

	const double threshold = rain.readDouble(L"FreqSimThreshold", 0.07);
	// 0.07 is a random constant that I feel appropriate

	std::vector<double> result;
	result.reserve(freqs.size());
	double lastValue = -1;
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

	return result;
}
