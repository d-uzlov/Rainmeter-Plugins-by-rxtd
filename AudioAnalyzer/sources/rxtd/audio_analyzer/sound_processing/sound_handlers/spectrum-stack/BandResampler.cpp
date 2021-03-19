// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "BandResampler.h"

#include "rxtd/LinearInterpolator.h"
#include "rxtd/audio_analyzer/audio_utils/CubicInterpolationHelper.h"
#include "rxtd/option_parsing/OptionList.h"

using rxtd::audio_analyzer::handler::BandResampler;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer BandResampler::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	auto bandsOption = context.options.get(L"bands");
	if (bandsOption.empty()) {
		context.log.error(L"bands option is not found");
		throw InvalidOptionsException{};
	}

	auto bandsLogger = context.log.context(L"bands: ");

	auto bandsArg = bandsOption.asSequence(L'(', L')', L',', bandsLogger);
	if (bandsArg.getSize() != 1) {
		bandsLogger.error(L"option must have exactly one value specified, one of: log, linear, custom");
		throw InvalidOptionsException{};
	}

	parseBandsElement(
		bandsArg.getElement(0).first.asIString(), bandsArg.getElement(0).second,
		params.bandFreqs, context.parser, bandsLogger
	);

	if (params.bandFreqs.size() < 2) {
		context.log.error(L"need >= 2 frequencies but only {} found", params.bandFreqs.size());
		throw InvalidOptionsException{};
	}

	params.useCubicResampling = context.parser.parse(context.options, L"cubicInterpolation").valueOr(true);

	return params;
}

void BandResampler::parseBandsElement(isview type, const Option& bandParams, std::vector<float>& freqs, Parser& parser, Logger& cl) {
	if (type == L"custom") {
		auto list = bandParams.asList(L',');
		if (list.size() < 2) {
			cl.error(L"custom must have at least two frequencies specified but {} found", list.size());
			throw InvalidOptionsException{};
		}
		freqs.reserve(static_cast<size_t>(list.size()));
		for (auto opt : list) {
			freqs.push_back(parser.parse(opt, L"bands: custom value").as<float>());
		}

		std::sort(freqs.begin(), freqs.end());

		float lastValue = -std::numeric_limits<float>::max();
		for (auto value : freqs) {
			if (value < 0.0f) {
				cl.error(L"frequencies must be >= 0 but {} found", value);
				throw InvalidOptionsException{};
			}
			if (std_fixes::MyMath::checkFloatEqual(value, lastValue)) {
				cl.error(L"frequencies must be distinct but {} and {} found", lastValue, value);
				throw InvalidOptionsException{};
			}

			lastValue = value;
		}

		return;
	}

	if (type != L"linear" && type != L"log") {
		cl.error(L"unknown type: {}", type);
		throw InvalidOptionsException{};
	}

	auto map = bandParams.asMap(L',', L' ');

	const index count = parser.parse(map.get(L"count"), L"bands: count").as<index>();
	if (count < 1) {
		cl.error(L"{}: count must be >= 1", type);
		throw InvalidOptionsException{};
	}

	const auto min = parser.parse(map.get(L"min"), L"bands: min").as<float>();
	const auto max = parser.parse(map.get(L"max"), L"bands: min").as<float>();
	if (max <= min) {
		cl.error(L"{}: max must be > min", type);
		throw InvalidOptionsException{};
	}

	if (type == L"linear") {
		const auto delta = max - min;

		for (index i = 0; i <= count; ++i) {
			freqs.push_back(min + delta * static_cast<float>(i) / static_cast<float>(count));
		}
	} else {
		// log
		const auto step = std::pow(2.0f, std::log2(max / min) / static_cast<float>(count));
		auto freq = min;
		freqs.push_back(freq);

		for (index i = 0; i < count; ++i) {
			freq *= step;
			freqs.push_back(freq);
		}
	}
}

HandlerBase::ConfigurationResult
BandResampler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	fftSource = dynamic_cast<FftAnalyzer*>(config.sourcePtr);
	if (fftSource == nullptr) {
		cl.error(L"invalid source, need FftAnalyzer");
		throw InvalidOptionsException{};
	}

	bandsCount = static_cast<index>(params.bandFreqs.size()) - 1;

	const index realCascadesCount = fftSource->getDataSize().layersCount;

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
	return { bandsCount, sourceDataSize.eqWaveSizes };
}

void BandResampler::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& config = getConfiguration();

	auto& source = *fftSource;
	const index layersCount = source.getDataSize().layersCount;

	float binWidth = static_cast<float>(config.sampleRate) / static_cast<float>(source.getFftSize());

	for (index cascadeIndex = 0; cascadeIndex < layersCount; ++cascadeIndex) {
		for (auto chunk : source.getChunks(cascadeIndex)) {
			auto dest = pushLayer(cascadeIndex);

			if (clock::now() > context.killTime) {
				auto myChunks = getChunks(cascadeIndex);
				myChunks.remove_suffix(1);
				dest.copyFrom(myChunks.empty() ? getSavedData(cascadeIndex) : myChunks.back());
				continue;
			}

			sampleCascade(chunk, dest, binWidth);
		}

		binWidth *= 0.5f;
	}
}

void BandResampler::sampleCascade(array_view<float> source, array_span<float> dest, float binWidth) {
	const index fftBinsCount = source.size();

	LinearInterpolator<float> lowerBinBoundInter;
	lowerBinBoundInter.setParams(
		-binWidth * 0.5f,
		(static_cast<float>(fftBinsCount) - 0.5f) * binWidth,
		0.0f,
		static_cast<float>(fftBinsCount - 1)
	);

	audio_utils::CubicInterpolationHelper cih;
	cih.setSource(source);

	for (index band = 0; band < bandsCount; band++) {
		const float bandMinFreq = params.bandFreqs[static_cast<size_t>(band)];
		const float bandMaxFreq = params.bandFreqs[static_cast<size_t>(band + 1)];

		if (params.useCubicResampling && bandMaxFreq - bandMinFreq < binWidth) {
			const float interpolatedCoordinate = lowerBinBoundInter.toValue((bandMinFreq + bandMaxFreq) * 0.5f);
			const float value = cih.getValueFor(interpolatedCoordinate);
			dest[band] = std::max(value, 0.0f);
		} else {
			const index minBin = static_cast<index>(std::floor(lowerBinBoundInter.toValue(bandMinFreq)));
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
	float binWidth = static_cast<float>(config.sampleRate) / static_cast<float>(fftSize);

	for (index i = 0; i < layerWeights.getBuffersCount(); ++i) {
		computeCascadeWeights(layerWeights[i], fftBinsCount, binWidth);
		binWidth *= 0.5f;
	}
}

void BandResampler::computeCascadeWeights(array_span<float> result, index fftBinsCount, float binWidth) {
	const float binWidthInverse = 1.0f / binWidth;

	const index bandsCount = static_cast<index>(params.bandFreqs.size()) - 1;

	const float fftMinFreq = -binWidth * 0.5f;
	const float fftMaxFreq = (static_cast<float>(fftBinsCount) - 0.5f) * binWidth;

	for (index band = 0; band < bandsCount; band++) {
		const float bandMaxFreq = std::min(params.bandFreqs[static_cast<size_t>(band + 1)], fftMaxFreq);
		if (bandMaxFreq < fftMinFreq) {
			result[band] = 0.0f;
			continue;
		}

		const float bandMinFreq = std::max(params.bandFreqs[static_cast<size_t>(band)], fftMinFreq);
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
	const ExternalMethods::CallContext& context
) {
	const index bandsCount = static_cast<index>(snapshot.bandFreqs.size()) - 1;

	if (prop == L"bandsCount") {
		context.printer.print(bandsCount);
		return true;
	}

	auto [nameOpt, valueOpt] = Option{ prop }.breakFirst(L' ');
	const auto ind = context.parser.parse(valueOpt, prop % csView()).as<index>();

	if (nameOpt.asIString() == L"lowerBound") {
		if (ind < static_cast<index>(snapshot.bandFreqs.size())) {
			context.printer.print(L"{}", snapshot.bandFreqs[static_cast<size_t>(ind)]);
			return true;
		}
		return false;
	}

	if (nameOpt.asIString() == L"centralFreq") {
		if (ind < static_cast<index>(snapshot.bandFreqs.size()) - 1) {
			auto result = snapshot.bandFreqs[static_cast<size_t>(ind)] + snapshot.bandFreqs[static_cast<size_t>(ind + 1)];
			result *= 0.5f;
			context.printer.print(L"{}", result);
			return true;
		}
		return false;
	}

	return false;
}
