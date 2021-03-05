// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "Color.h"
#include "rxtd/std_fixes/Vector2D.h"

namespace rxtd::audio_analyzer::image_utils {
	class StripedImageFadeHelper {
		std_fixes::Vector2D<IntColor> resultBuffer{};

		index borderSize = 0;
		index pastLastStripIndex{};
		double fading = 0.0;

		IntColor background{};
		IntColor border{};

	public:
		void setParams(
			IntColor _background,
			index _borderSize, IntColor _border,
			double _fading
		) {
			background = _background;
			borderSize = _borderSize;
			border = _border;
			fading = _fading;
		}

		void setPastLastStripIndex(index value) {
			pastLastStripIndex = value;
		}

		[[nodiscard]]
		std_fixes::array2d_view<IntColor> getResultBuffer() const {
			return resultBuffer;
		}

		void inflate(std_fixes::array2d_view<IntColor> source);

		void drawBorderInPlace(std_fixes::array2d_span<IntColor> source) const;

	private:
		void inflateLine(array_view<IntColor> source, array_span<IntColor> dest) const;

		void drawBorderInLine(array_span<IntColor> line) const;
	};
}
