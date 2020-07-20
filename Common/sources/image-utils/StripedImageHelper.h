/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"
#include "array2d_view.h"

namespace rxtd::utils {
	class StripedImageHelper {
	public:
		using PixelColor = uint32_t;

	protected:
		std::vector<PixelColor> pixelData { };
		index width = 0;
		index height = 0;
		index beginningOffset = 0;
		index maxOffset = 0;

		PixelColor backgroundValue = { };
		PixelColor lastFillValue = { };
		index sameStripsCount = 0;

	public:
		// Must be called before #setDimensions
		void setBackground(PixelColor value);

		void setDimensions(index width, index height);

		void setWidth(index value) {
			setDimensions(value, height);
		}

		void setHeight(index value) {
			setDimensions(width, value);
		}

		void pushStrip(array_view<PixelColor> stripData);

		void pushEmptyLine(PixelColor value);

		void pushEmptyStrip(array_view<PixelColor> stripData);

		bool isEmpty() const {
			return sameStripsCount >= width;
		}

		array2d_view<PixelColor> getPixels() const {
			return getCurrentLinesArray();
		}

	private:
		static index getReserveSize(index size);

		array2d_span<PixelColor> getCurrentLinesArray();

		array2d_view<PixelColor> getCurrentLinesArray() const;

		void incrementStrip();
	};

}
