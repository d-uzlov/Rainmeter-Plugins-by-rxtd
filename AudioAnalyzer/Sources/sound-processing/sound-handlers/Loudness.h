/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "RainmeterWrappers.h"
#include "../../audio-utils/InfiniteResponseFilter.h"

namespace rxtd::audio_analyzer {
	class KWeightingFilterBuilder {
	public:
		static audio_utils::InfiniteResponseFilter create1(double samplingFrequency);
		static audio_utils::InfiniteResponseFilter create2(double samplingFrequency);
	};

	class Loudness : public SoundHandler {
	public:
		struct Params {
		private:
			friend Loudness;

			istring resamplerId;

		};
	private:
		Params params { };

		// index samplesPerSec { };

		audio_utils::InfiniteResponseFilter filter1 { };
		audio_utils::InfiniteResponseFilter filter2 { };

		std::vector<float> intermediateWave { };
		float result = 0.0;

		mutable string propString { };

		// todo
		bool valid = true;
		bool changed = true;

	public:
		void setParams(Params params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override { };

		bool isValid() const override {
			return true;
		}
		array_view<float> getData(layer_t layer) const override {
			return { &result, 1 };
		}
		layer_t getLayersCount() const override {
			return 1;
		}

		const wchar_t* getProp(const isview& prop) const override;

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl);

	private:
		void preprocessWave();
		double calculateLoudness();
	};

}
