/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WeightedBlur.h"
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

const std::vector<double>& WeightedBlur::GaussianCoefficientsManager::forSigma(double sigma) {
	const auto radius = std::clamp<index>(std::lround(sigma * 3.0), minRadius, maxRadius);

	auto& vec = blurCoefficients[radius];
	if (!vec.empty()) {
		return vec;
	}

	vec = generateGaussianKernel(radius);
	return vec;
}

const std::vector<double>& WeightedBlur::GaussianCoefficientsManager::forMaximumRadius() {
	auto& vec = blurCoefficients[maxRadius];
	if (!vec.empty()) {
		return vec;
	}

	vec = generateGaussianKernel(maxRadius);
	return vec;
}

void WeightedBlur::GaussianCoefficientsManager::setRadiusBounds(index min, index max) {
	minRadius = min;
	maxRadius = max;
}

std::vector<double> WeightedBlur::GaussianCoefficientsManager::generateGaussianKernel(index radius) {

	std::vector<double> kernel;
	kernel.resize(radius * 2ll + 1);

	const double restoredSigma = radius * (1.0 / 3.0);
	const double powerFactor = 1.0 / (2.0 * restoredSigma * restoredSigma);

	index r = -radius;
	double sum = 0.0;
	for (double& k : kernel) {
		k = std::exp(-r * r * powerFactor);
		sum += k;
		r++;
	}
	const double sumInverse = 1.0 / sum;
	for (auto& c : kernel) {
		c *= sumInverse;
	}

	return kernel;
}

std::optional<WeightedBlur::Params> WeightedBlur::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	//                                                                      ?? ↓↓ looks best ?? at 0.25 ↓↓ ?? // TODO
	params.radiusMultiplier = std::max<double>(optionMap.get(L"radiusMultiplier"sv).asFloat(1.0) * 0.25, 0.0);

	params.minRadius = std::max<double>(optionMap.get(L"minRadius"sv).asFloat(1.0), 0.0);
	params.maxRadius = std::max<double>(optionMap.get(L"maxRadius"sv).asFloat(20.0), params.minRadius);

	params.minRadiusAdaptation = std::max<double>(optionMap.get(L"MinRadiusAdaptation"sv).asFloat(2.0), 0.0);
	params.maxRadiusAdaptation = std::max<double>(
		optionMap.get(L"MaxRadiusAdaptation"sv).asFloat(params.minRadiusAdaptation), 0.0);

	// params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0), std::numeric_limits<float>::epsilon());
	params.minWeight = 0.0; // doesn't work as expected

	return params;
}

void WeightedBlur::setParams(Params _params, Channel channel) {
	params = std::move(_params);
	setResamplerID(params.sourceId);
	setValid(true);
}

void WeightedBlur::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void WeightedBlur::_finish(const DataSupplier& dataSupplier) {
	if (!changed) {
		return;
	}

	source = dataSupplier.getHandler<ResamplerProvider>(params.sourceId);
	if (source == nullptr) {
		setValid(false);
		return;
	}

	blurData(dataSupplier);
	changed = false;
}

array_view<float> WeightedBlur::getData(layer_t layer) const {
	return blurredValues[layer];
}

SoundHandler::layer_t WeightedBlur::getLayersCount() const {
	return layer_t(blurredValues.size());
}

void WeightedBlur::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

void WeightedBlur::reset() {
	changed = true;
}

void WeightedBlur::blurData(const DataSupplier& dataSupplier) {
	const BandResampler* resamplerPtr = getResampler(dataSupplier);
	if (resamplerPtr == nullptr) {
		setValid(false);
		return;
	}

	const BandResampler& resampler = *resamplerPtr;
	blurredValues.resize(source->getLayersCount());

	double minRadius = params.minRadius * std::pow(params.minRadiusAdaptation, resampler.getStartingLayer());
	double maxRadius = params.maxRadius * std::pow(params.maxRadiusAdaptation, resampler.getStartingLayer());

	for (layer_t cascade = 0; cascade < source->getLayersCount(); ++cascade) {
		const auto cascadeMagnitudes = source->getData(cascade);
		const auto cascadeWeights = resampler.getBandWeights(cascade);
		const auto bandsCount = cascadeMagnitudes.size();

		gcm.setRadiusBounds(
			std::min<index>(std::lround(minRadius), bandsCount),
			std::min<index>(std::lround(maxRadius), bandsCount)
		);
		auto& cascadeValues = blurredValues[cascade];
		cascadeValues.resize(bandsCount);

		for (index band = 0; band < bandsCount; ++band) {
			const auto weight = cascadeWeights[band];
			auto& kernel = weight > std::numeric_limits<float>::epsilon()
				               ? gcm.forSigma(params.radiusMultiplier / weight)
				               : gcm.forMaximumRadius();
			if (kernel.size() < 2) {
				cascadeValues[band] = cascadeMagnitudes[band];
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
				result += (cascadeWeights[bandIndex] > params.minWeight ? 1.0 : 0.0) * kernel[kernelIndex] *
					cascadeMagnitudes[bandIndex];

				kernelIndex++;
				bandIndex++;
			}

			cascadeValues[band] = result;
		}

		minRadius *= params.minRadiusAdaptation;
		maxRadius *= params.maxRadiusAdaptation;
	}
}
