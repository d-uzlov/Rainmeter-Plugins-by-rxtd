/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	class LogarithmicValueMapper : public SoundHandler {
	public:
		struct Params {
		private:
			friend LogarithmicValueMapper;

			istring sourceId;

			double offset;
			double sensitivity;
		};

	private:
		Params params{ };

		index samplesPerSec{ };

		double logNormalization{ };

		std::vector<std::vector<float>> resultValues;

		bool changed = true;
		bool valid = false;

		mutable string propString{ };

	public:

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl);

		void setParams(Params _params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;


		bool isValid() const override {
			return valid;
		}

		array_view<float> getData(layer_t layer) const override {
			return resultValues[layer];
		}

		layer_t getLayersCount() const override {
			return layer_t(resultValues.size());
		}

		const wchar_t* getProp(const isview& prop) const override;

	private:
		void updateValues(const DataSupplier& dataSupplier);
		void transformToLog(const SoundHandler& source);
	};
}
