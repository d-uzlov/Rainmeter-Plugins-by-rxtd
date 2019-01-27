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
#include "OptionParser.h"
#include "RainmeterWrappers.h"

namespace rxaa {
	class BlockMean : public SoundHandler {
	public:
		struct Params {
			double attackTime { };
			double decayTime { };
			double resolution { };
		};
	protected:
		Params params { };

		index samplesPerSec { };

		double attackDecayConstants[2] { };
		index blockSize { };

		index counter = 0;
		double intermediateResult = 0.0;
		double result = 0.0;

		string propString { };

	public:
		void setParams(Params params);

		const double* getData() const override;
		index getCount() const override;
		void setSamplesPerSec(index samplesPerSec) override;
		const wchar_t* getProp(const sview& prop) override;
		void reset() override;
		void processSilence(const DataSupplier& dataSupplier) override;

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl);

	private:
		void recalculateConstants();
		virtual void finish() = 0;
	};

	class BlockRms : public BlockMean {
	public:
		void process(const DataSupplier& dataSupplier) override;

	private:
		void finish() override;
	};

	class BlockPeak : public BlockMean {
	public:
		void process(const DataSupplier& dataSupplier) override;

	private:
		void finish() override;
	};
}
