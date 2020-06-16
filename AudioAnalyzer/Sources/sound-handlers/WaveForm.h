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
#include <Vector2D.h>
#include "OptionParser.h"
#include "RainmeterWrappers.h"
#include "MutableLinearInterpolator.h"

namespace rxtd::audio_analyzer {
	class WaveForm : public SoundHandler {
	public:
		enum class LineDrawingPolicy {
			NEVER,
			BELOW_WAVE,
			ALWAYS,
		};

		struct Params {
		private:
			friend WaveForm;

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
		double min { };
		double max { };
		bool changed = false;

		mutable string propString { };

		utils::Vector2D<uint32_t> imageBuffer;
		string filepath { };

		utils::MutableLinearInterpolator interpolator;

	public:
		void setParams(const Params& _params);

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl, const utils::Rainmeter& rain);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		bool isValid() const override {
			return true; // TODO
		}
		array_view<float> getData(layer_t layer) const override {
			return { };
		}
		layer_t getLayersCount() const override {
			return 0;
		}

		const wchar_t* getProp(const isview& prop) const override;
		bool isStandalone() override {
			return true;
		}

	private:
		void updateParams();
		void writeFile(const DataSupplier& dataSupplier);
		void fillLine();
	};
}
