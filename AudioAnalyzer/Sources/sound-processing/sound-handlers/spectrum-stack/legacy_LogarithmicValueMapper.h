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
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	class legacy_LogarithmicValueMapper : public SoundHandler {
	public:
		struct Params {
		private:
			friend legacy_LogarithmicValueMapper;

			istring sourceId;

			double offset;
			double sensitivity;
		};

	private:
		Params params{ };

		index samplesPerSec{ };

		double logNormalization{ };

		SoundHandler* source{ };

		std::vector<std::vector<float>> resultValues;

		bool changed = true;

		std::vector<LayerData> layers;

	public:

		static std::optional<Params> parseParams(const OptionMap& optionMap, Logger& cl);

		void setParams(const Params& params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void _process(const DataSupplier& dataSupplier) override;
		void _finish() override;

		LayeredData getData() const override {
			return layers;
		}

	protected:
		[[nodiscard]]
		isview getSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		bool vCheckSources(Logger& cl) override;

	private:
		void updateValues();
		void transformToLog(SoundHandler& source);
	};
}
