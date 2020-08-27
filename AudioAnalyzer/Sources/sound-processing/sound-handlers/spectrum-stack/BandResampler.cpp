/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandResampler.h"

#include "LinearInterpolator.h"
#include "../../../audio-utils/CubicInterpolationHelper.h"
#include "option-parser/OptionMap.h"
#include "option-parser/OptionList.h"

using namespace std::string_literals;

using namespace audio_analyzer;

SoundHandler::ParseResult BandResampler::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return { };
	}

	const auto freqListIndex = om.get(L"freqList").asString();
	if (freqListIndex.empty()) {
		cl.error(L"freqList is not found");
		return { };
	}

	params.bandFreqs = parseFreqList(freqListIndex, rain);
	if (params.bandFreqs.empty()) {
		return { };
	}

	params.minCascade = std::max(om.get(L"minCascade").asInt(0), 0);
	params.maxCascade = std::max(om.get(L"maxCascade").asInt(0), 0);

	if (params.minCascade > params.maxCascade) {
		cl.error(
			L"max cascade must be >= min cascade but max={} < min={} are found",
			params.maxCascade, params.minCascade
		);
		return { };
	}

	if (legacyNumber < 104) {
		params.includeDC = om.get(L"includeDC").asBool(true);
		params.legacy_proportionalValues = om.get(L"proportionalValues").asBool(true);
	} else {
		params.includeDC = true;
		params.legacy_proportionalValues = false;
	}

	const bool defaultCubicResampling = !(legacyNumber < 104);
	params.useCubicResampling = om.get(L"cubicInterpolation").asBool(defaultCubicResampling);

	ParseResult result;
	result.setParams(std::move(params));
	result.addSource(sourceId);
	result.setPropGetter(getProp);
	return result;
}

std::vector<float> BandResampler::parseFreqList(sview listId, const Rainmeter& rain) {
	auto freqListOptionName = L"FreqList-"s += listId;
	auto freqListOption = rain.read(freqListOptionName);
	if (freqListOption.empty()) {
		freqListOptionName = L"FreqList_"s += listId;
		freqListOption = rain.read(freqListOptionName);
	}

	Logger cl = rain.createLogger().context(L"FreqList {}: ", listId);
	if (freqListOption.empty()) {
		cl.error(L"description is not found");
		return { };
	}

	std::vector<float> freqs;

	for (auto boundOption : freqListOption.asList(L'|')) {
		auto options = boundOption.asList(L' ');
		auto type = options.get(0).asIString();

		if (type == L"custom") {
			if (options.size() < 2) {
				cl.error(L"custom must have at least two frequencies specified but {} found", options.size());
				return { };
			}
			for (index i = 1; i < options.size(); ++i) {
				freqs.push_back(options.get(i).asFloatF());
			}
			continue;
		}

		if (type != L"linear" && type != L"log") {
			cl.error(L"unknown list type '{}'", type);
			return { };
		}

		if (options.size() != 4) {
			cl.error(L"{} must have 3 options (count, min, max)", type);
			return { };
		}

		const index count = options.get(1).asInt(0);
		if (count < 1) {
			cl.error(L"count must be >= 1");
			return { };
		}

		const auto min = options.get(2).asFloatF();
		const auto max = options.get(3).asFloatF();
		if (max <= min) {
			cl.error(L"max must be > min");
			return { };
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
	}

	std::sort(freqs.begin(), freqs.end());

	const double threshold = rain.readDouble(L"FreqSimThreshold", 0.07);
	// 0.07 is a random constant that I feel appropriate

	std::vector<float> result;
	result.reserve(freqs.size());
	float lastValue = -1;
	for (auto value : freqs) {
		if (value <= 0) {
			cl.error(L"frequencies must be > 0 but {} found", value);
			return { };
		}
		if (value - lastValue < threshold) {
			continue;
		}

		result.push_back(value);
		lastValue = value;
	}

	if (result.size() < 2) {
		cl.error(L"need >= 2 frequencies but only {} found", result.size());
		return { };
	}

	return result;
}

SoundHandler::ConfigurationResult
BandResampler::vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();
	fftSource = dynamic_cast<FftAnalyzer*>(config.sourcePtr);
	if (fftSource == nullptr) {
		cl.error(L"invalid source, need FftAnalyzer");
		return { };
	}

	const auto cascadesCount = fftSource->getDataSize().layersCount;

	if (params.minCascade > cascadesCount) {
		cl.error(L"minCascade is more than number of cascades");
		return { };
	}

	bandsCount = index(params.bandFreqs.size() - 1);

	if (params.legacy_proportionalValues) {
		legacy_generateBandMultipliers();
	}

	startCascade = 1;
	endCascade = cascadesCount + 1;
	if (params.minCascade > 0) {
		startCascade = params.minCascade;

		if (params.maxCascade >= params.minCascade && cascadesCount >= params.maxCascade) {
			endCascade = params.maxCascade + 1;
		}
	}
	startCascade--;
	endCascade--;

	const index realCascadesCount = endCascade - startCascade;

	layerWeights.setBuffersCount(realCascadesCount);
	layerWeights.setBufferSize(bandsCount);

	computeWeights(fftSource->getFftSize());

	bandWeights.setBuffersCount(bandsCount);
	bandWeights.setBufferSize(realCascadesCount);
	for (index i = 0; i < bandsCount; ++i) {
		for (index j = 0; j < realCascadesCount; ++j) {
			bandWeights[i][j] = layerWeights[j][i];
		}
	}

	if (nullptr == std::any_cast<Snapshot>(&snapshotAny)) {
		snapshotAny = Snapshot{ };
	}
	auto& snapshot = *std::any_cast<Snapshot>(&snapshotAny);
	snapshot.bandFreqs = params.bandFreqs;

	return { realCascadesCount, bandsCount };
}

void BandResampler::vProcess(array_view<float> wave, clock::time_point killTime) {
	auto& config = getConfiguration();

	auto& source = *fftSource;

	double binWidth = static_cast<double>(config.sampleRate) / (source.getFftSize() * std::pow(2, startCascade));

	for (index cascadeIndex = startCascade; cascadeIndex < endCascade; ++cascadeIndex) {
		const index localCascadeIndex = cascadeIndex - startCascade;

		for (auto chunk : source.getChunks(cascadeIndex)) {
			auto dest = pushLayer(localCascadeIndex, chunk.equivalentWaveSize);

			if (clock::now() > killTime) {
				array_view<float> dataToCopy;
				auto myChunks = getChunks(cascadeIndex);
				dataToCopy = myChunks.empty() ? getSavedData(cascadeIndex) : myChunks.back().data;
				std::copy(dataToCopy.begin(), dataToCopy.end(), dest.begin());
				continue;
			}

			sampleCascade(chunk.data, dest, binWidth);

			if (params.legacy_proportionalValues) {
				for (index band = 0; band < bandsCount; ++band) {
					dest[band] *= legacy_bandFreqMultipliers[band];
				}
			}
		}

		binWidth *= 0.5;
	}
}

void BandResampler::sampleCascade(array_view<float> source, array_span<float> dest, double binWidth) {
	const index fftBinsCount = source.size();

	utils::LinearInterpolator lowerBinBoundInter;
	lowerBinBoundInter.setParams(
		-binWidth * 0.5,
		(fftBinsCount - 0.5) * binWidth,
		double(0),
		double(fftBinsCount - 1)
	);

	audio_utils::CubicInterpolationHelper cih;
	cih.setSource(source);

	for (index band = 0; band < bandsCount; band++) {
		const double bandMinFreq = params.bandFreqs[band];
		const double bandMaxFreq = params.bandFreqs[band + 1];

		if (params.useCubicResampling && bandMaxFreq - bandMinFreq < binWidth) {
			const double interpolatedCoordinate = lowerBinBoundInter.toValue((bandMinFreq + bandMaxFreq) * 0.5);
			const double value = cih.getValueFor(interpolatedCoordinate);
			dest[band] = std::max<float>(value, 0.0f);
		} else {
			const index minBin = index(std::floor(lowerBinBoundInter.toValue(bandMinFreq)));
			if (minBin >= fftBinsCount) {
				break;
			}
			const index maxBin = std::min(index(std::floor(lowerBinBoundInter.toValue(bandMaxFreq))), fftBinsCount - 1);

			float value = 0.0;
			for (index bin = minBin; bin <= maxBin; ++bin) {
				value += source[bin];
			}
			const float binsCount = (1.0f + maxBin - minBin);
			dest[band] = value / binsCount;
		}
	}
}

void BandResampler::computeWeights(index fftSize) {
	auto& config = getConfiguration();
	const auto fftBinsCount = fftSize / 2;
	double binWidth = static_cast<double>(config.sampleRate) / (fftSize * std::pow(2, startCascade));

	for (index i = 0; i < layerWeights.getBuffersCount(); ++i) {
		computeCascadeWeights(layerWeights[i], fftBinsCount, binWidth);
		binWidth *= 0.5;
	}
}

void BandResampler::computeCascadeWeights(array_span<float> result, index fftBinsCount, double binWidth) {
	const double binWidthInverse = 1.0 / binWidth;

	const index bandsCount = params.bandFreqs.size() - 1;

	const float fftMinFreq = float(params.includeDC ? -binWidth * 0.5 : binWidth * 0.5);
	const float fftMaxFreq = float((fftBinsCount - 0.5) * binWidth);

	for (index band = 0; band < bandsCount; band++) {
		const float bandMaxFreq = std::min(params.bandFreqs[band + 1], fftMaxFreq);
		if (bandMaxFreq < fftMinFreq) {
			result[band] = 0.0f;
			continue;
		}

		const float bandMinFreq = std::max(params.bandFreqs[band], fftMinFreq);
		if (bandMinFreq > fftMaxFreq) {
			result[band] = 0.0f;
			continue;
		}

		const double bandWidth = bandMaxFreq - bandMinFreq;
		result[band] = float(bandWidth * binWidthInverse);
	}
}

void BandResampler::legacy_generateBandMultipliers() {
	legacy_bandFreqMultipliers.resize(bandsCount);
	double multipliersSum{ };
	for (index i = 0; i < bandsCount; ++i) {
		legacy_bandFreqMultipliers[i] = std::log(params.bandFreqs[i + 1] - params.bandFreqs[i] + 1.0f);
		// bandFreqMultipliers[i] = params.bandFreqs[i + 1] - params.bandFreqs[i];
		multipliersSum += legacy_bandFreqMultipliers[i];
	}
	const double bandFreqMultipliersAverage = multipliersSum / bandsCount;
	const double multiplierCorrectingConstant = 1.0 / bandFreqMultipliersAverage;
	for (auto& multiplier : legacy_bandFreqMultipliers) {
		multiplier = float(multiplier * multiplierCorrectingConstant);
	}
}

bool BandResampler::getProp(const std::any& handlerSpecificData, isview prop, utils::BufferPrinter& printer) {
	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);

	const index bandsCount = snapshot.bandFreqs.size();

	if (prop == L"bands count") {
		printer.print(bandsCount);
		return true;
	}

	auto index = legacy_parseIndexProp(prop, L"lower bound", bandsCount + 1);
	if (index == -2) {
		printer.print(L"0");
		return true;
	}
	if (index >= 0) {
		if (index > 0) {
			index--;
		}
		printer.print(snapshot.bandFreqs[index]);
		return true;
	}

	index = legacy_parseIndexProp(prop, L"upper bound", bandsCount + 1);
	if (index == -2) {
		printer.print(L"0");
		return true;
	}
	if (index >= 0) {
		if (index > 0) {
			index--;
		}
		printer.print(snapshot.bandFreqs[index + 1]);
		return true;
	}

	index = legacy_parseIndexProp(prop, L"central frequency", bandsCount + 1);
	if (index == -2) {
		printer.print(L"0");
		return true;
	}
	if (index >= 0) {
		if (index > 0) {
			index--;
		}
		printer.print((snapshot.bandFreqs[index] + snapshot.bandFreqs[index + 1]) * 0.5);
		return true;
	}

	return false;
}
