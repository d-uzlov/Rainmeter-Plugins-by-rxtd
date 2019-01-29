/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandAnalyzer.h"
#include <cmath>
#include "FftAnalyzer.h"
#include "FastMath.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

const std::vector<double>& rxaa::BandAnalyzer::GaussianCoefficientsManager::forSigma(double sigma) {
	const auto radius = std::clamp<index>(std::lround(sigma * 3.0), minRadius, maxRadius);

	auto &vec = blurCoefficients[radius];
	if (!vec.empty()) {
		return vec;
	}

	vec = generateGaussianKernel(radius, sigma);
	return vec;
}

void rxaa::BandAnalyzer::GaussianCoefficientsManager::setRadiusBounds(index min, index max) {
	minRadius = min;
	maxRadius = max;
}

std::vector<double> rxaa::BandAnalyzer::GaussianCoefficientsManager::generateGaussianKernel(index radius, double sigma) {

	std::vector<double> kernel;
	kernel.resize(radius * 2ll + 1);

	const double powerFactor = 1.0 / (2.0 * sigma * sigma);

	index r = -radius;
	double sum = 0.0;
	for (double& k : kernel) {
		k = std::exp(-r * r * powerFactor);
		sum += k;
		r++;
	}
	const double sumInverse = 1.0 / sum;
	for (auto &c : kernel) {
		c *= sumInverse;
	}

	return kernel;
}

std::optional<rxaa::BandAnalyzer::Params> rxaa::BandAnalyzer::parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl, utils::Rainmeter &rain) {
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

	utils::OptionParser optionParser;
	const auto bounds = optionParser.asList(rain.readString(L"FreqList_"s += freqListIndex), L'|');
	utils::Rainmeter::ContextLogger freqListLogger { rain.getLogger() };
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

	params.minCascade = std::max<cascade_t>(optionMap.get(L"minCascade"sv).asInt<cascade_t>(0), 0);
	params.maxCascade = std::max<cascade_t>(optionMap.get(L"maxCascade"sv).asInt<cascade_t>(0), 0);

	params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0.1), std::numeric_limits<float>::epsilon());
	params.targetWeight = std::max<double>(optionMap.get(L"targetWeight"sv).asFloat(1.5), std::numeric_limits<float>::epsilon());
	params.weightFallback = std::clamp(optionMap.get(L"weightFallback"sv).asFloat(0.5), 0.0, 1.0) * params.targetWeight;

	params.zeroLevel = std::max<double>(optionMap.get(L"zeroLevelMultiplier"sv).asFloat(1.0), 0.0)
		* std::numeric_limits<float>::epsilon();
	params.zeroLevelHard = std::clamp<double>(optionMap.get(L"zeroLevelHardMultiplier"sv).asFloat(0.01), 0.0, 1.0)
		* params.zeroLevel;
	params.zeroWeight = std::max<double>(optionMap.get(L"zeroWeightMultiplier"sv).asFloat(1.0), 0.0)
		* std::numeric_limits<float>::epsilon();


	params.includeDC = optionMap.get(L"includeDC"sv).asBool(true);
	params.proportionalValues = optionMap.get(L"proportionalValues"sv).asBool(true);

	//                                                                          ?? ↓↓ looks best ?? at 0.25 ↓↓ ??
	params.blurRadiusMultiplier = std::max<double>(optionMap.get(L"blurRadiusMultiplier"sv).asFloat(1.0) * 0.25, 0.0);

	params.minBlurRadius = std::max<index>(optionMap.get(L"minBlurRadius"sv).asInt(1), 0);
	params.maxBlurRadius = std::max<index>(optionMap.get(L"maxBlurRadius"sv).asInt(10), params.minBlurRadius);

	params.blurMinAdaptation = std::max<double>(optionMap.get(L"blurMinAdaptation"sv).asFloat(2.0), 1.0);
	params.blurMaxAdaptation = std::max<double>(optionMap.get(L"blurMaxAdaptation"sv).asFloat(params.blurMinAdaptation), 1.0);

	params.sensitivity = std::clamp<double>(optionMap.get(L"sensitivity"sv).asFloat(35.0), std::numeric_limits<float>::epsilon(), 1000.0);
	params.offset = optionMap.get(L"offset"sv).asFloat(0.0);

	params.smoothingFactor = optionMap.get(L"smoothingFactor"sv).asInt(4);
	if (params.smoothingFactor <= 0) {
		cl.warning(L"smoothingFactor should be >= 1 but {} found, assume 1", params.smoothingFactor);
		params.smoothingFactor = 1;
	}

	auto smoothingCurveString = optionMap.get(L"smoothingCurve"sv).asIString();
	params.exponentialFactor = 1;
	if (smoothingCurveString.empty() || smoothingCurveString == L"exponential") {
		params.smoothingCurve = SmoothingCurve::EXPONENTIAL;
		params.exponentialFactor = optionMap.get(L"exponentialFactor").asFloat(1.5);
	} else if (smoothingCurveString == L"flat") {
		params.smoothingCurve = SmoothingCurve::FLAT;
	} else if (smoothingCurveString == L"linear") {
		params.smoothingCurve = SmoothingCurve::LINEAR;
	} else {
		cl.warning(L"smoothingCurve '{}' now recognized, assume 'flat'", smoothingCurveString);
		params.smoothingCurve = SmoothingCurve::FLAT;
	}

	params.mixFunction = optionMap.get(L"mixFunction"sv).asIString(L"product") == L"product" ? MixFunction::PRODUCT : MixFunction::AVERAGE;

	return params;
}

void rxaa::BandAnalyzer::setParams(Params _params) {
	this->params = std::move(_params);

	source = nullptr;

	bandsCount = params.bandFreqs.size() - 1;

	if (bandsCount < 1) {
		return;
	}

	bandFreqMultipliers.resize(bandsCount);
	double multipliersSum { };
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

	logNormalization = 20.0 / params.sensitivity;

	values.resize(bandsCount);
	pastValues.resize(params.smoothingFactor);
	for (auto &v : pastValues) {
		v.resize(bandsCount);
	}

	lastFilteringTime = { };

	analysisComputed = false;
}

void rxaa::BandAnalyzer::process(const DataSupplier& dataSupplier) {
	if (params.bandFreqs.size() < 2) {
		return;
	}

	source = dynamic_cast<const FftAnalyzer*>(dataSupplier.getHandler(params.fftId));
	changed = true;
}

void rxaa::BandAnalyzer::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void rxaa::BandAnalyzer::finish(const DataSupplier& dataSupplier) {
	if (changed) {
		updateValues();
		changed = false;
	}
}

const double* rxaa::BandAnalyzer::getData() const {
	return values.data();
}

index rxaa::BandAnalyzer::getCount() const {
	if (params.bandFreqs.size() < 2) {
		return 0;
	}
	return params.bandFreqs.size() - 1;
}

void rxaa::BandAnalyzer::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
	analysisComputed = false;
}

const wchar_t* rxaa::BandAnalyzer::getProp(const isview& prop) const {
	propString.clear();

	const auto bandsCount = params.bandFreqs.size() - 1;

	if (prop == L"bands count") {
		propString = std::to_wstring(bandsCount);
	} else if (prop == L"cascade analysis") {
		return analysis.analysisString.c_str();
	} else if (prop == L"min cascade used") {
		propString = std::to_wstring(analysis.minCascadeUsed + 1);
	} else if (prop == L"max cascade used") {
		propString = std::to_wstring(analysis.maxCascadeUsed + 1);
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

void rxaa::BandAnalyzer::reset() {
	changed = true;
}


void rxaa::BandAnalyzer::updateValues() {
	if (bandsCount < 1 || source == nullptr) {
		return;
	}

	if (!analysisComputed) {
		const auto cascadesCount = source->getCascadesCount();

		cascade_t cascadeIndexBegin = 1;
		cascade_t cascadeIndexEnd = cascadesCount + 1;
		if (params.minCascade > 0) {
			if (cascadesCount >= params.minCascade) {
				cascadeIndexBegin = params.minCascade;

				if (params.maxCascade >= params.minCascade && cascadesCount >= params.maxCascade) {
					cascadeIndexEnd = params.maxCascade + 1;
				}
			} else {
				return;
			}
		}
		cascadeIndexBegin--;
		cascadeIndexEnd--;

		computeBandInfo(cascadeIndexBegin, cascadeIndexEnd);
		computeAnalysis(cascadeIndexBegin, cascadeIndexEnd);
	}

	if (analysis.minCascadeUsed < 0) {
		return;
	}

	sampleData();

	if (params.blurRadiusMultiplier != 0.0) {
		blurData();
	}

	gatherData();
	applyTimeFiltering();
	transformToLog();
}

void rxaa::BandAnalyzer::computeBandInfo(cascade_t cascadeIndexBegin, cascade_t cascadeIndexEnd) {
	cascadesInfo.resize(cascadeIndexEnd - cascadeIndexBegin);
	for (auto &cascadeInfo : cascadesInfo) {
		cascadeInfo.setSize(bandsCount);
	}

	const auto fftBinsCount = source->getCount();
	double binWidth = static_cast<double>(samplesPerSec) / (source->getFftSize() * std::pow(2, cascadeIndexBegin));
	double binWidthInverse = 1.0 / binWidth;

	for (auto&[_, cascadeBandInfo] : cascadesInfo) {
		index bin = params.includeDC ? 0 : 1; // bin 0 is ~DC
		index band = 0;

		double bandMinFreq = params.bandFreqs[0];
		double bandMaxFreq = params.bandFreqs[1];

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
				cascadeBandInfo[band].weight += weight;
			}

			if (bandMaxFreq >= binUpperFreq) {
				bin++;
			} else {
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

		for (auto &info : cascadeBandInfo) {
			if (info.weight >= std::numeric_limits<float>::epsilon()) { // float on purpose
				info.blurSigma = params.blurRadiusMultiplier / info.weight;
			} else {
				info.blurSigma = 0.0;
			}
		}
	}

}

void rxaa::BandAnalyzer::computeAnalysis(cascade_t startCascade, cascade_t endCascade) {
	if (analysisComputed) {
		return;
	}

	std::wostringstream out;

	out.precision(1);
	out << std::fixed;

	analysis.minCascadeUsed = -1;
	analysis.maxCascadeUsed = -1;

	analysis.weightError = false;

	analysis.bandEndCascades.resize(values.size());

	for (index band = 0; band < index(values.size()); ++band) {
		double weight = 0.0;
		cascade_t bandMinCascade = -1;
		cascade_t bandMaxCascade = -1;

		for (cascade_t cascade = startCascade; cascade < endCascade; ++cascade) {
			const auto &info = cascadesInfo[cascade - startCascade].bandsInfo[band];

			if (info.weight >= params.minWeight) {
				weight += info.weight;
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

	const cascade_t minVectorIndex = analysis.minCascadeUsed - startCascade;
	const cascade_t maxVectorIndex = analysis.maxCascadeUsed - startCascade;

	if (maxVectorIndex + 1 < cascade_t(cascadesInfo.size())) {
		cascadesInfo.erase(cascadesInfo.begin() + (maxVectorIndex + 1), cascadesInfo.end());
	}
	if (minVectorIndex > 0) {
		cascadesInfo.erase(cascadesInfo.begin(), cascadesInfo.begin() + (minVectorIndex - 1));
	}

	analysisComputed = true;
}

void rxaa::BandAnalyzer::sampleData() {
	const cascade_t cascadeIndexBegin = analysis.minCascadeUsed;
	const cascade_t cascadeIndexEnd = analysis.maxCascadeUsed + 1;

	const auto fftBinsCount = source->getCount();
	double binWidth = static_cast<double>(samplesPerSec) / (source->getFftSize() * std::pow(2u, cascadeIndexBegin));
	double binWidthInverse = 1.0 / binWidth;

	for (auto cascade = cascadeIndexBegin; cascade < cascadeIndexEnd; ++cascade) {
		index bin = params.includeDC ? 0 : 1; // bin 0 is ~DC
		index band = 0;

		double bandMinFreq = params.bandFreqs[0];
		double bandMaxFreq = params.bandFreqs[1];

		const auto fftData = source->getCascade(cascade);

		auto &cascadeMagnitudes = cascadesInfo[cascade - cascadeIndexBegin].magnitudes;
		std::fill(cascadeMagnitudes.begin(), cascadeMagnitudes.end(), 0.0);

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
				cascadeMagnitudes[band] += fftValue * weight;
			}

			if (bandMaxFreq >= binUpperFreq) {
				bin++;
			} else {
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

void rxaa::BandAnalyzer::blurData() {
	cascadeTempBuffer.resize(bandsCount);

	double minRadius = params.minBlurRadius * std::pow(2.0, analysis.minCascadeUsed);
	double maxRadius = params.maxBlurRadius * std::pow(2.0, analysis.minCascadeUsed);

	for (auto&[cascadeMagnitudes, cascadeBandInfo] : cascadesInfo) {

		gcm.setRadiusBounds(index(minRadius), index(maxRadius));

		for (index band = 0; band < bandsCount; ++band) {
			const double sigma = cascadeBandInfo[band].blurSigma;
			if (sigma == 0.0) {
				cascadeTempBuffer[band] = cascadeMagnitudes[band];
				continue;
			}

			auto &kernel = gcm.forSigma(sigma);
			if (kernel.size() < 2) {
				cascadeTempBuffer[band] = cascadeMagnitudes[band];
				continue;
			}

			index radius = kernel.size() >> 1;
			index bandStartIndex = band - radius;
			index kernelStartIndex = 0;

			if (bandStartIndex < 0) {
				kernelStartIndex = -bandStartIndex;
				bandStartIndex = 0;
			}

			double result = 0.0;
			index kernelIndex = kernelStartIndex;
			index bandIndex = bandStartIndex;

			while (true) {
				if (bandIndex >= bandsCount || kernelIndex >= index(kernel.size())) {
					break;
				}
				result += kernel[kernelIndex] * cascadeMagnitudes[bandIndex];

				kernelIndex++;
				bandIndex++;
			}

			cascadeTempBuffer[band] = result;
		}

		minRadius *= params.blurMinAdaptation;
		maxRadius *= params.blurMaxAdaptation;

		std::swap(cascadeMagnitudes, cascadeTempBuffer);
	}
}

void rxaa::BandAnalyzer::gatherData() {
	pastValuesIndex++;
	if (pastValuesIndex >= params.smoothingFactor) {
		pastValuesIndex = 0;
	}

	for (index band = 0; band < index(values.size()); ++band) {
		double weight = 0.0;
		cascade_t cascadesSummed = 0;
		const cascade_t bandEndCascade = analysis.bandEndCascades[band] - analysis.minCascadeUsed;

		double value;
		if (params.mixFunction == MixFunction::PRODUCT) {
			value = 1.0;
		} else {
			value = 0.0;
		}

		for (cascade_t cascade = 0; cascade < bandEndCascade; cascade++) {
			const auto bandWeight = cascadesInfo[cascade].bandsInfo[band].weight;

			const auto magnitude = cascadesInfo[cascade].magnitudes[band];
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
			for (cascade_t cascade = 0; cascade < bandEndCascade; cascade++) {
				const auto bandWeight = cascadesInfo[cascade].bandsInfo[band].weight;

				const auto magnitude = cascadesInfo[cascade].magnitudes[band];
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
		} else if (params.mixFunction == MixFunction::PRODUCT) {
			value = 0.0;
		}


		if (params.proportionalValues) {
			value *= bandFreqMultipliers[band];
		}

		pastValues[pastValuesIndex][band] = value;
	}
}

void rxaa::BandAnalyzer::applyTimeFiltering() {
	if (params.smoothingFactor <= 1) {
		std::swap(values, pastValues[0]); // pastValues consists of only one array
		return;
	}

	auto startPastIndex = pastValuesIndex + 1;
	if (startPastIndex >= params.smoothingFactor) {
		startPastIndex = 0;
	}
	switch (params.smoothingCurve) {
	case SmoothingCurve::FLAT:
		for (index band = 0; band < index(values.size()); ++band) {
			double outValue = 0.0;
			for (index i = 0; i < params.smoothingFactor; ++i) {
				outValue += pastValues[i][band];
			}
			outValue /= params.smoothingFactor;
			values[band] = outValue;
		}
		break;

	case SmoothingCurve::LINEAR:
		for (index band = 0; band < index(values.size()); ++band) {
			double outValue = 0.0;
			index smoothingWeight = 0;
			index valueWeight = 1;

			for (index i = startPastIndex; i < params.smoothingFactor; ++i) {
				outValue += pastValues[i][band] * valueWeight;
				smoothingWeight += valueWeight;
				valueWeight++;
			}
			for (index i = 0; i < startPastIndex; ++i) {
				outValue += pastValues[i][band] * valueWeight;
				smoothingWeight += valueWeight;
				valueWeight++;
			}

			outValue /= smoothingWeight;
			values[band] = outValue;
		}
		break;

	case SmoothingCurve::EXPONENTIAL:
		for (index band = 0; band < index(values.size()); ++band) {
			double outValue = 0.0;
			double smoothingWeight = 0;
			double weight = 1;

			for (index i = startPastIndex; i < params.smoothingFactor; ++i) {
				outValue += pastValues[i][band] * weight;
				smoothingWeight += weight;
				weight *= params.exponentialFactor;
			}
			for (index i = 0; i < startPastIndex; ++i) {
				outValue += pastValues[i][band] * weight;
				smoothingWeight += weight;
				weight *= params.exponentialFactor;
			}

			outValue /= smoothingWeight;
			values[band] = outValue;
		}
		break;

	default: std::terminate(); // should be unreachable statement
	}
}

void rxaa::BandAnalyzer::transformToLog() {
	constexpr double log10inverse = 0.30102999566398119521; // 1.0 / log2(10)

	for (index i = 0; i < bandsCount; ++i) {
		double value = values[i];

		value = utils::FastMath::log2(float(value)) * log10inverse;
		value = value * logNormalization + 1.0;
		value += params.offset;
		values[i] = value;
	}
}

std::optional<std::vector<double>> rxaa::BandAnalyzer::parseFreqList(const utils::OptionParser::OptionList& bounds, utils::Rainmeter::ContextLogger& cl, const utils::Rainmeter &rain) {
	std::vector<double> freqs;

	utils::OptionParser optionParser;
	for (auto bound : bounds.viewCI()) {
		if (bound.find(L"linear ") == 0 || bound.find(L"log ") == 0) {

			auto options = optionParser.asList(bound, L' ');
			if (options.size() != 4) {
				cl.error(L"linear/log must have 3 options (count, min, max)");
				return std::nullopt;
			}
			index count;
			std::wstringstream(string { options.get(1) }) >> count; // TODO rewrite

			if (count < 1) {
				cl.error(L"count must be >= 1");
				return std::nullopt;
			}

			const double min = options.getOption(2).asFloat();
			const double max = options.getOption(3).asFloat();
			if (min >= max) {
				cl.error(L"min must be < max");
				return std::nullopt;
			}

			if (bound.find(L"linear ") == 0) {
				const double delta = max - min;

				for (index i = 0; i <= count; ++i) {
					freqs.push_back(min + delta * i / count);
				}
			} else {
				// log
				const auto step = std::pow(2.0, std::log2(max / min) / count);
				double freq = min;
				freqs.push_back(freq);

				for (auto i = 0ll; i < count; ++i) {
					freq *= step;
					freqs.push_back(freq);
				}
			}
			continue;
		}
		if (bound.find(L"custom ") == 0) {
			auto options = optionParser.asList(bound, L' ');
			if (options.size() < 2) {
				cl.error(L"custom must have at least two frequencies specified but {} found", options.size());
				return std::nullopt;
			}
			for (auto i = 1u; i < options.size(); ++i) {
				freqs.push_back(options.getOption(i).asFloat());
			}
		}

		cl.error(L"unknown list type '{}'", bound);
		return std::nullopt;
	}

	std::sort(freqs.begin(), freqs.end());

	const double threshold = rain.readDouble(L"FreqSimThreshold", 0.07); // 0.07 is a random constant that I feel appropriate

	std::vector<double> result;
	result.reserve(freqs.size());
	double lastValue = -1;
	for (auto value : freqs) {
		if (value <= 0) {
			cl.error(L"frequency must be > 0 ({} found)", value);
			return std::nullopt;
		}
		if (std::abs(value - lastValue) < threshold) {
			lastValue = value;
			continue;
		}
		result.push_back(value);
		lastValue = value;
	}

	return result;
}
