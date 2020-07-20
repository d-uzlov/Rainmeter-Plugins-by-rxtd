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
	template <typename PIXEL_VALUE_TYPE>
	class StripedImage {
	public:
		using PixelValueType = PIXEL_VALUE_TYPE;

	protected:
		std::vector<PixelValueType> pixelData { };
		index width = 0;
		index height = 0;
		index beginningOffset = 0;
		index maxOffset = 0;

		PixelValueType backgroundValue = { };
		PixelValueType lastFillValue = { };
		index sameStripsCount = 0;

	public:
		// Must be called before #setDimensions
		void setBackground(PixelValueType value) {
			backgroundValue = value;
		}

		void setDimensions(index width, index height) {
			if (this->width == width && this->height == height) {
				return;
			}
			this->width = width;
			this->height = height;

			beginningOffset = 0;

			const index vectorSize = width * height;
			maxOffset = getReserveSize(vectorSize);

			pixelData.reserve(vectorSize + maxOffset);

			auto imageLines = getCurrentLinesArray();
			imageLines.init(backgroundValue);

			lastFillValue = backgroundValue;
			sameStripsCount = imageLines.getBuffersCount();
		}

		void setWidth(index value) {
			setDimensions(value, height);
		}

		void setHeight(index value) {
			setDimensions(width, value);
		}

		void pushStrip(array_view<PixelValueType> stripData) {
			sameStripsCount = 0;

			incrementStrip();

			auto imageLines = getCurrentLinesArray();
			const index lastStripIndex = width - 1;
			for (index i = 0; i < stripData.size(); i++) {
				imageLines[i][lastStripIndex] = stripData[i];
			}
		}

		void pushEmptyLine(PixelValueType value) {
			if (sameStripsCount >= width) {
				return;
			}

			if (sameStripsCount == 0 || lastFillValue != value) {
				lastFillValue = value;
				sameStripsCount = 1;
			} else {
				sameStripsCount++;
			}

			incrementStrip();

			auto imageLines = getCurrentLinesArray();
			const index lastStripIndex = width - 1;
			for (index i = 0; i < height; i++) {
				imageLines[i][lastStripIndex] = value;
			}
		}

		bool isEmpty() const {
			return sameStripsCount >= width;
		}

		array2d_view<PixelValueType> getPixels() const {
			return getCurrentLinesArray();
		}

	private:
		static index getReserveSize(index size) {
			constexpr double reserveCoef = 0.5;
			return static_cast<index>(std::ceil(size * reserveCoef));
		}

		array2d_span<PixelValueType> getCurrentLinesArray() {
			return { pixelData.data() + beginningOffset, height, width };
		}

		array2d_view<PixelValueType> getCurrentLinesArray() const {
			return { pixelData.data() + beginningOffset, height, width };
		}

		void incrementStrip() {
			if (beginningOffset < maxOffset) {
				beginningOffset++;
				return;
			}

			std::copy(
				pixelData.data() + maxOffset + 1, // this +1 is very important. Without it image won't shift enough
				pixelData.data() + maxOffset + width * height,
				pixelData.data()
			);
			beginningOffset = 0;
		}
	};
}
