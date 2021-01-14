/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "StripedImage.h"
#include "Color.h"

namespace rxtd::utils {
	class StripedImageFadeHelper {
		Vector2D<IntColor> resultBuffer{};

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
		array2d_view<IntColor> getResultBuffer() const {
			return resultBuffer;
		}

		void inflate(array2d_view<IntColor> source);

		void drawBorderInPlace(array2d_span<IntColor> source) const;

	private:
		void inflateLine(array_view<IntColor> source, array_span<IntColor> dest) const;

		void drawBorderInLine(array_span<IntColor> line) const;
	};
}
