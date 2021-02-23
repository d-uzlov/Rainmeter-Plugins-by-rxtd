/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandResampler.h"

#include "LinearInterpolator.h"
#include "audio-utils/CubicInterpolationHelper.h"
#include "option-parsing/OptionList.h"

using rxtd::audio_analyzer::handler::BandResampler;
using rxtd::audio_analyzer::handler::HandlerBase;

using namespace std::string_literals;

rxtd::audio_analyzer::handler::ParamsContainer BandResampler::vParseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	Version version
) const {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return {};
	}

	if (om.get(L"bands").empty()) {
		cl.error(L"bands option is not found");
		return {};
	}

	params.bandFreqs = parseFreqList(om.get(L"bands"), cl.context(L"bands: "));

	if (params.bandFreqs.size() < 2) {
		cl.error(L"need >= 2 frequencies but only {} found", params.bandFreqs.size());
		return {};
	}

	params.minCascade = std::max(om.get(L"minCascade").asInt(0), 0);
	params.maxCascade = std::max(om.get(L"maxCascade").asInt(0), 0);

	if (params.minCascade > params.maxCascade) {
		cl.error(
			L"max cascade must be >= min cascade but max={} < min={} are found",
			params.maxCascade, params.minCascade
		);
		return {};
	}

	params.useCubicResampling = om.get(L"cubicInterpolation").asBool(true);

	return result;
}

bool BandResampler::parseFreqListElement(OptionList& options, std::vector<float>& freqs, Logger& cl) {
	auto type = options.get(0).asIString();

	if (type == L"custom") {
		if (options.size() < 2) {
			cl.error(L"custom must have at least two frequencies specified but {} found", options.size());
			return {};
		}
		for (index i = 1; i < options.size(); ++i) {
			freqs.push_back(options.get(i).asFloatF());
		}
		return true;
	}

	if (type != L"linear" && type != L"log") {
		cl.error(L"unknown type '{}'", type);
		return {};
	}

	if (options.size() != 4) {
		cl.error(L"{} must have 3 options (count, min, max)", type);
		return {};
	}

	const index count = options.get(1).asInt(0);
	if (count < 1) {
		cl.error(L"count must be >= 1");
		return {};
	}

	const auto min = options.get(2).asFloatF();
	const auto max = options.get(3).asFloatF();
	if (max <= min) {
		cl.error(L"max must be > min");
		return {};
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

	return true;
}

std::vector<float> BandResampler::parseFreqList(Option freqListOption, Logger& cl) {
	std::vector<float> freqs;

	auto options = freqListOption.asList(L' ');
	const auto success = parseFreqListElement(options, freqs, cl);
	if (!success) {
		return {};
	}

	return makeBandsFromFreqs(freqs, cl);
}

std::vector<float> BandResampler::makeBandsFromFreqs(array_span<float> freqs, Logger& cl) {
	std::sort(freqs.begin(), freqs.end());

	const float threshold = std::numeric_limits<float>::epsilon();

	std::vector<float> result;
	result.reserve(freqs.size());
	float lastValue = -1;
	for (auto value : freqs) {
		if (value <= 0) {
			cl.error(L"frequencies must be > 0 but {} found", value);
			return {};
		}
		if (value - lastValue < threshold) {
			continue;
		}

		result.push_back(value);
		lastValue = value;
	}

	return result;
}

HandlerBase::ConfigurationResult
BandResampler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	fftSource = dynamic_cast<FftAnalyzer*>(config.sourcePtr);
	if (fftSource == nullptr) {
		cl.error(L"invalid source, need FftAnalyzer");
		return {};
	}

	const auto cascadesCount = fftSource->getDataSize().layersCount;

	if (params.minCascade > cascadesCount) {
		cl.error(L"minCascade is more than number of cascades");
		return {};
	}

	bandsCount = index(params.bandFreqs.size() - 1);

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

	auto& snapshot = externalData.clear<Snapshot>();
	snapshot.bandFreqs = params.bandFreqs;

	auto& sourceDataSize = fftSource->getDataSize();
	std::vector<index> eqWS;
	for (index i = startCascade; i < endCascade; i++) {
		eqWS.push_back(sourceDataSize.eqWaveSizes[i]);
	}
	return { bandsCount, std::move(eqWS) };
}

void BandResampler::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& config = getConfiguration();

	auto& source = *fftSource;

	float binWidth = static_cast<float>(config.sampleRate) / (static_cast<float>(source.getFftSize()) * std::powf(2.0f, static_cast<float>(startCascade)));

	for (index cascadeIndex = startCascade; cascadeIndex < endCascade; ++cascadeIndex) {
		const index localCascadeIndex = cascadeIndex - startCascade;

		for (auto chunk : source.getChunks(cascadeIndex)) {
			auto dest = pushLayer(localCascadeIndex);

			if (clock::now() > context.killTime) {
				auto myChunks = getChunks(cascadeIndex);
				myChunks.remove_suffix(1);
				dest.copyFrom(myChunks.empty() ? getSavedData(cascadeIndex) : myChunks.back());
				continue;
			}

			sampleCascade(chunk, dest, binWidth);
		}

		binWidth *= 0.5;
	}
}

void BandResampler::sampleCascade(array_view<float> source, array_span<float> dest, float binWidth) {
	const index fftBinsCount = source.size();

	utils::LinearInterpolator<float> lowerBinBoundInter;
	lowerBinBoundInter.setParams(
		-binWidth * 0.5f,
		(static_cast<float>(fftBinsCount) - 0.5f) * binWidth,
		0.0f,
		static_cast<float>(fftBinsCount - 1)
	);

	audio_utils::CubicInterpolationHelper cih;
	cih.setSource(source);

	for (index band = 0; band < bandsCount; band++) {
		const float bandMinFreq = params.bandFreqs[band];
		const float bandMaxFreq = params.bandFreqs[band + 1];

		if (params.useCubicResampling && bandMaxFreq - bandMinFreq < binWidth) {
			const float interpolatedCoordinate = lowerBinBoundInter.toValue((bandMinFreq + bandMaxFreq) * 0.5f);
			const float value = static_cast<float>(cih.getValueFor(interpolatedCoordinate));
			dest[band] = std::max(value, 0.0f);
		} else {
			const index minBin = index(std::floor(lowerBinBoundInter.toValue(bandMinFreq)));
			if (minBin >= fftBinsCount) {
				break;
			}
			const index maxBin = std::min(static_cast<index>(std::floor(lowerBinBoundInter.toValue(bandMaxFreq))), fftBinsCount - 1);

			float value = 0.0f;
			for (index bin = minBin; bin <= maxBin; ++bin) {
				value += source[bin];
			}
			const float binsCount = (1.0f + static_cast<float>(maxBin) - static_cast<float>(minBin));
			dest[band] = value / binsCount;
		}
	}
}

void BandResampler::computeWeights(index fftSize) {
	auto& config = getConfiguration();
	const index fftBinsCount = fftSize / 2;
	float binWidth = static_cast<float>(config.sampleRate) / (static_cast<float>(fftSize) * std::powf(2.0f, static_cast<float>(startCascade)));

	for (index i = 0; i < layerWeights.getBuffersCount(); ++i) {
		computeCascadeWeights(layerWeights[i], fftBinsCount, binWidth);
		binWidth *= 0.5f;
	}
}

void BandResampler::computeCascadeWeights(array_span<float> result, index fftBinsCount, float binWidth) {
	const float binWidthInverse = 1.0f / binWidth;

	const index bandsCount = params.bandFreqs.size() - 1;

	const float fftMinFreq = -binWidth * 0.5f;
	const float fftMaxFreq = (static_cast<float>(fftBinsCount) - 0.5f) * binWidth;

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

		const float bandWidth = bandMaxFreq - bandMinFreq;
		result[band] = bandWidth * binWidthInverse;
	}
}

bool BandResampler::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternalMethods::CallContext& context
) {
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
