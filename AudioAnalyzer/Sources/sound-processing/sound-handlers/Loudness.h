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
#include "../../audio-utils/LogarithmicIRF.h"

namespace rxtd::audio_analyzer {
	class KWeightingFilterBuilder {
	public:
		static audio_utils::InfiniteResponseFilter createHighShelf(double samplingFrequency);
		static audio_utils::InfiniteResponseFilter createHighPass(double samplingFrequency);
	};

	class Loudness : public SoundHandler {
		// based on EBU R 128
		// see:
		//   https://books.google.ru/books?id=wYNiDwAAQBAJ&pg=PT402&lpg=PT402&source=bl&ots=b_IYgSnzH_&sig=ACfU3U24oCdbQZLqFmaH7sFO39CpaoRZVQ&hl=en&sa=X&ved=2ahUKEwjMobfksaDqAhVxx4sKHaRSBToQ6AEwAnoECAoQAQ#v=onepage&f=false
		//   https://github.com/BrechtDeMan/loudness.py/blob/master/loudness.py
		//   https://hydrogenaud.io/index.php?topic=86116.25

	public:
		struct Params {
		private:
			friend Loudness;

			double attackTime { };
			double decayTime { };
		};
	private:
		Params params { };

		index samplesPerSec { };

		audio_utils::LogarithmicIRF filter { };
		index blockSize = 0;

		audio_utils::InfiniteResponseFilter highShelfFilter { };
		audio_utils::InfiniteResponseFilter highPassFilter { };

		std::vector<float> intermediateWave { };
		float result = 0.0;

		mutable string propString { };

	public:
		void setParams(Params params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override { }

		array_view<float> getData(layer_t layer) const override {
			return { &result, 1 };
		}

		const wchar_t* getProp(const isview& prop) const override;

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl);

	private:
		void updateFilter(index blockSize);
		void preprocessWave();
		double calculateLoudness();
	};

}
