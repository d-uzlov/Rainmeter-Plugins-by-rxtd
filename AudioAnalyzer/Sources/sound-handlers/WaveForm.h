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
#include "Color.h"
#include <ContinuousBuffersHolder.h>
#include "OptionParser.h"
#include "RainmeterWrappers.h"

namespace rxtd::utils {
	class Rainmeter;
}

namespace rxaa {
	class WaveForm : public SoundHandler {
	public:
		enum class LineDrawingPolicy {
			NEVER,
			BELOW_WAVE,
			ALWAYS,
		};

		struct Params {
			double resolution { };
			index width { };
			index height { };
			string prefix = L".";
			utils::Color backgroundColor { };
			utils::Color waveColor { };
			utils::Color lineColor { };
			LineDrawingPolicy lineDrawingPolicy { };
			double gain { };
		};

	private:
		index samplesPerSec { };

		Params params;

		index blockSize { };
		uint32_t backgroundInt { };
		uint32_t waveInt { };
		uint32_t lineInt { };

		index counter = 0;
		index lastIndex = 0;
		double result = 0.0;
		double min { };
		double max { };

		string propString { };

		utils::ContinuousBuffersHolder<uint32_t> imageBuffer;
		string filepath { };

		class MutableLinearInterpolator {
			double valMin = 0;
			double linMin = 0;
			double alpha = 1;
			double reverseAlpha = 1;

		public:
			MutableLinearInterpolator() = default;

			MutableLinearInterpolator(double linMin, double linMax, double valMin, double valMax) {
				setParams(linMin, linMax, valMin, valMax);
			}

			void setParams(double linMin, double linMax, double valMin, double valMax) {
				this->linMin = linMin;
				this->valMin = valMin;
				this->alpha = (valMax - valMin) / (linMax - linMin);
				this->reverseAlpha = 1 / alpha;
			}

			double toValue(double linear) const {
				return valMin + (linear - linMin) * alpha;
			}

			double toLinear(double value) const {
				return linMin + (value - valMin) * reverseAlpha;
			}
		} interpolator;

	public:
		void setParams(const Params& _params);
		// void setParams(double resolution, unsigned width, unsigned height, string prefix, rxu::Color backgroundColor, rxu::Color waveColor, rxu::Color lineColor, LineDrawingPolicy lineDrawingPolicy, double gain);

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl, const utils::Rainmeter& rain);

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		const double* getData() const override;
		index getCount() const override;
		void setSamplesPerSec(index samplesPerSec) override;
		const wchar_t* getProp(const sview& prop) override;
		void reset() override;

	private:
		void updateParams();
		void writeFile(const DataSupplier& dataSupplier);
		void fillLine();
	};
}
