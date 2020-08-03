/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "UniformBlur.h"
#include "option-parser/OptionMap.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::vector<double> UniformBlur::GaussianCoefficientsManager::generateGaussianKernel(index radius) {

	std::vector<double> kernel;
	kernel.resize(radius * 2ll + 1);

	const double restoredSigma = radius * (1.0 / 3.0);
	const double powerFactor = 1.0 / (2.0 * restoredSigma * restoredSigma);

	double r = -double(radius);
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

bool UniformBlur::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	//                                                        ?? ↓↓ looks best ?? at 0.25 ↓↓ ?? // TODO
	params.blurRadius = std::max<double>(optionMap.get(L"Radius"sv).asFloat(1.0) * 0.25, 0.0);
	params.blurRadiusAdaptation = std::max<double>(optionMap.get(L"RadiusAdaptation"sv).asFloat(2.0), 0.0);

	return true;
}

SoundHandler::LinkingResult UniformBlur::vFinishLinking(Logger& cl) {
	const auto source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return { };
	}

	const auto dataSize = source->getDataSize();
	return dataSize;
}

void UniformBlur::vProcess(const DataSupplier& dataSupplier) {
	changed = true;
}

void UniformBlur::vFinish() {
	if (!changed) {
		return;
	}
	changed = false;

	auto& source = *getSource();
	source.finish();
	const auto sourceData = source.getData();

	double theoreticalRadius = params.blurRadius * std::pow(params.blurRadiusAdaptation, source.getStartingLayer());

	const auto refIds = getRefIds();

	const index cascadesCount = source.getDataSize().layersCount;
	for (index i = 0; i < cascadesCount; ++i) {
		const auto sid = sourceData.ids[i];

		if (sid != refIds[i]) {
			const auto cascadeSource = sourceData.values[i];
			auto cascadeResult = updateLayerData(i, sid);

			const index radius = std::llround(theoreticalRadius);
			if (radius < 1) {
				std::copy(cascadeSource.begin(), cascadeSource.end(), cascadeResult.begin());
			} else {
				blurCascade(cascadeSource, cascadeResult, radius);
			}
		}

		theoreticalRadius *= params.blurRadiusAdaptation;
	}
}

void UniformBlur::blurCascade(array_view<float> source, array_span<float> dest, index radius) {
	auto& kernel = gcm.forRadius(radius);

	const auto bandsCount = source.size();
	for (index i = 0; i < bandsCount; ++i) {

		index bandStartIndex = i - radius;
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
			result += kernel[kernelIndex] * source[bandIndex];

			kernelIndex++;
			bandIndex++;
		}

		dest[i] = float(result);
	}
}
