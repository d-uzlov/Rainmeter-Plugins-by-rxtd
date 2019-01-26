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
#include <ContinuousBuffersHolder.h>
#include "Color.h"
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxaa {
	class Spectrogram : public SoundHandler {
	public:
		struct Params {
			double resolution { };
			unsigned length { };
			std::wstring sourceName { };
			std::wstring prefix = { };
			rxu::Color baseColor { };
			rxu::Color maxColor { };
		};

	private:
		Params params;

		uint32_t samplesPerSec { };

		unsigned blockSize { };

		unsigned int counter = 0;
		std::size_t lastIndex = 0;
		unsigned int sourceSize = 0;
		double result = 0.0;

		std::wstring propString { };

		rxu::ContinuousBuffersHolder<uint32_t> buffer;
		std::wstring filepath { };

	public:
		void setParams(const Params& _params);
		// void setParams(double resolution, unsigned length, std::wstring_view sourceName, std::wstring prefix, rxu::Color baseColor, rxu::Color maxColor);

		static std::optional<Params> parseParams(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger &cl, const rxu::Rainmeter& rain);

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		const double* getData() const override;
		size_t getCount() const override;
		void setSamplesPerSec(uint32_t samplesPerSec) override;
		const wchar_t* getProp(const std::wstring_view& prop) override;
		void reset() override;

	private:
		void updateParams();
		void writeFile(const DataSupplier& dataSupplier);
		void fillLine(const double* data);
	};
}
