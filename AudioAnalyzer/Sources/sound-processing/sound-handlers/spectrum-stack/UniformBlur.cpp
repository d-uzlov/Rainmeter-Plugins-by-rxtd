﻿/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "UniformBlur.h"
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<UniformBlur::Params> UniformBlur::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	//                                                        ?? ↓↓ looks best ?? at 0.25 ↓↓ ?? // TODO
	params.blurRadius = std::max<double>(optionMap.get(L"Radius"sv).asFloat(1.0) * 0.25, 0.0);
	params.blurRadiusAdaptation = std::max<double>(optionMap.get(L"RadiusAdaptation"sv).asFloat(2.0), 0.0);

	return params;
}

const std::vector<double>& UniformBlur::GaussianCoefficientsManager::forRadius(index radius) {
	auto& vec = blurCoefficients[radius];
	if (!vec.empty()) {
		return vec;
	}

	vec = generateGaussianKernel(radius);
	return vec;
}

std::vector<double> UniformBlur::GaussianCoefficientsManager::generateGaussianKernel(index radius) {

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

void UniformBlur::setParams(const Params& _params, Channel channel) {
	params = _params;
}

void UniformBlur::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void UniformBlur::_finish() {
	if (!changed) {
		return;
	}

	source->finish();
	blurData(*source);
	changed = false;
}

void UniformBlur::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

void UniformBlur::reset() {
	changed = true;
}

bool UniformBlur::vCheckSources(Logger& cl) {
	source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	return true;
}

void UniformBlur::blurData(const SoundHandler& source) {
	const auto sourceData = source.getData();
	const auto layersCount = sourceData.size();
	
	blurredValues.resize(layersCount);

	double theoreticalRadius = params.blurRadius * std::pow(params.blurRadiusAdaptation, source.getStartingLayer());

	for (index cascade = 0; cascade < layersCount; ++cascade) {
		const auto cascadeMagnitudes = sourceData[cascade].values;
		const auto bandsCount = cascadeMagnitudes.size();

		auto& cascadeValues = blurredValues[cascade];
		cascadeValues.resize(bandsCount);

		const index radius = std::llround(theoreticalRadius);
		if (radius < 1) {
			std::copy(cascadeMagnitudes.begin(), cascadeMagnitudes.end(), cascadeValues.begin());
		} else {
			auto& kernel = gcm.forRadius(radius);

			for (index band = 0; band < bandsCount; ++band) {

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

				cascadeValues[band] = float(result);
			}
		}

		theoreticalRadius *= params.blurRadiusAdaptation;
	}
	
	layers.resize(layersCount);
	for (index i = 0; i < layersCount; ++i) {
		layers[i].id++;
		layers[i].values = blurredValues[i];
	}
}
