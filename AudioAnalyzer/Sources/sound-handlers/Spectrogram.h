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
#include <Vector2D.h>
#include "Color.h"
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxaa {
	class Spectrogram : public SoundHandler {
	public:
		struct Params {
		private:
			friend Spectrogram;

			double resolution { };
			index length { };
			istring sourceName { };
			string prefix = { };
			utils::Color baseColor { };
			utils::Color maxColor { };
		};

	private:
		Params params;

		index samplesPerSec { };

		index blockSize { };

		index counter = 0;
		index lastIndex = 0;
		index sourceSize = 0;
		double result = 0.0;
		bool changed = false;

		mutable string propString { };

		utils::Vector2D<uint32_t> buffer;
		string filepath { };

	public:
		void setParams(const Params& _params);

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl, const utils::Rainmeter& rain);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		const double* getData() const override;
		index getCount() const override;

		const wchar_t* getProp(const isview& prop) const override;
		bool isStandalone() override {
			return true;
		}

	private:
		void updateParams();
		void writeFile(const DataSupplier& dataSupplier);
		void fillLine(const double* data);
	};
}
