/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "BandResampler.h"
#include "Vector2D.h"

namespace rxtd::audio_analyzer {
	class FiniteTimeFilter : public ResamplerProvider {
	public:
		enum class SmoothingCurve {
			FLAT,
			LINEAR,
			EXPONENTIAL,
		};
		struct Params {
		private:
			friend FiniteTimeFilter;

			istring sourceId;

			SmoothingCurve smoothingCurve;
			index smoothingFactor;
			double exponentialFactor;
		};

	private:
		Params params { };

		index samplesPerSec { };

		// pastValues[Layer][FilterSize][Band]
		// std::vector<std::vector<std::vector<float>>> pastValues;
		std::vector<utils::Vector2D<float>> pastValues;
		// std::vector<std::vector<float>> values;
		utils::Vector2D<float> values;
		index pastValuesIndex = 0;

		double smoothingNormConstant { };

		bool changed = true;
		bool valid = false;
		const SoundHandler* source = nullptr;
		const BandResampler* resampler = nullptr;

		mutable string propString { };

	public:

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl);

		void setParams(Params _params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		bool isValid() const override {
			return valid;
		}
		array_view<float> getData(layer_t layer) const override {
			if (params.smoothingFactor <= 1) {
				return source->getData(layer);
			}

			return values[layer];
		}
		layer_t getLayersCount() const override {
			if (params.smoothingFactor <= 1) {
				return source->getLayersCount();
			}

			return layer_t(values.getBuffersCount());
		}
		const BandResampler* getResampler() const override {
			if (!valid) {
				return nullptr;
			}

			return resampler;
		}

		const wchar_t* getProp(const isview& prop) const override;

	private:
		void adjustSize();
		void copyValues();
		void applyTimeFiltering();
	};
}
