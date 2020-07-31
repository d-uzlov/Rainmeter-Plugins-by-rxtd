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

std::optional<WeightedBlur::Params> WeightedBlur::parseParams(const OptionMap& optionMap, Logger& cl) {
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

void WeightedBlur::setParams(const Params& _params, Channel channel) {
	params = _params;
}

void WeightedBlur::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void WeightedBlur::_finish() {
	if (!changed) {
		return;
	}

	source->finish();
	blurData();
	changed = false;
}

array_view<float> WeightedBlur::getData(index layer) const {
	return blurredValues[layer];
}

index WeightedBlur::getLayersCount() const {
	return index(blurredValues.size());
}

void WeightedBlur::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

void WeightedBlur::reset() {
	changed = true;
}

bool WeightedBlur::vCheckSources(Logger& cl) {
	resamplerPtr = getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return false;
	}

	source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	return true;
}

void WeightedBlur::blurData() {
	const BandResampler& resampler = *resamplerPtr;
	blurredValues.resize(source->getLayersCount());

	double minRadius = params.minRadius * std::pow(params.minRadiusAdaptation, resampler.getStartingLayer());
	double maxRadius = params.maxRadius * std::pow(params.maxRadiusAdaptation, resampler.getStartingLayer());

	for (index cascade = 0; cascade < source->getLayersCount(); ++cascade) {
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

			cascadeValues[band] = float(result);
		}

		minRadius *= params.minRadiusAdaptation;
		maxRadius *= params.maxRadiusAdaptation;
	}
}
