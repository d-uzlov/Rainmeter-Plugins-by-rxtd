/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "ResamplerProvider.h"
#include "Vector2D.h"

namespace rxtd::audio_analyzer {
	class UniformBlur : public ResamplerProvider {
	public:
		struct Params {
			istring sourceId;

			double blurRadius{ };
			double blurRadiusAdaptation{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.sourceId == rhs.sourceId
					&& lhs.blurRadius == rhs.blurRadius
					&& lhs.blurRadiusAdaptation == rhs.blurRadiusAdaptation;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		class GaussianCoefficientsManager {
			// radius -> coefs vector
			std::unordered_map<index, std::vector<double>> blurCoefficients;
		public:
			const std::vector<double>& forRadius(index radius) {
				auto& vec = blurCoefficients[radius];
				if (vec.empty()) {
					vec = generateGaussianKernel(radius);
				}

				return vec;
			}

		private:
			static std::vector<double> generateGaussianKernel(index radius);
		};

		Params params{ };
		GaussianCoefficientsManager gcm;

		utils::Vector2D<float> values;
		std::vector<LayerData> layers;

		bool changed = true;

	public:
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const override;

		const Params& getParams() const {
			return params;
		}

		void setParams(const Params& value) {
			params = value;
		}

	protected:
		[[nodiscard]]
		isview vGetSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		bool vFinishLinking(Logger& cl) override;

	public:
		void vReset() override;
		void vProcess(const DataSupplier& dataSupplier) override;
		void vFinish() override;

		LayeredData vGetData() const override {
			return layers;
		}

		[[nodiscard]]
		DataSize getDataSize() const override {
			return { values.getBuffersCount(), values.getBufferSize() };
		}

	private:
		void blurCascade(array_view<float> source, array_span<float> dest, index radius);
	};
}
