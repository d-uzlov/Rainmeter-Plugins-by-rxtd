/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "BmpWriter.h"

namespace rxtd::utils {
	class TransposedLinedImageHelper {
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
		mutable bool emptinessWritten = false;

	public:
		// Must be called before #setDimensions
		void setBackground(PixelColor value) {
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
			emptinessWritten = false;
		}

		void setWidth(index value) {
			setDimensions(value, height);
		}

		void setHeight(index value) {
			setDimensions(width, value);
		}

		void pushStrip(array_view<PixelColor> stripData) {
			sameStripsCount = 0;
			emptinessWritten = false;

			incrementStrip();

			auto imageLines = getCurrentLinesArray();
			const index lastStripIndex = width - 1;
			for (index i = 0; i < stripData.size(); i++) {
				imageLines[i][lastStripIndex] = stripData[i];
			}
		}

		void pushEmptyLine(PixelColor value) {
			if (sameStripsCount >= width) {
				return;
			}

			if (sameStripsCount == 0 || lastFillValue != value) {
				lastFillValue = value;
				sameStripsCount = 1;
			} else {
				sameStripsCount++;
			}
			emptinessWritten = false;

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

		void write(const string& filepath) const {
			if (emptinessWritten) {
				return;
			}

			utils::BmpWriter::writeFile(filepath, getCurrentLinesArray());

			if (isEmpty()) {
				emptinessWritten = true;
			}
		}

	private:
		static index getReserveSize(index size) {
			constexpr double reserveCoef = 0.5;
			return static_cast<index>(std::ceil(size * reserveCoef));
		}

		utils::array2d_span<PixelColor> getCurrentLinesArray() {
			return { pixelData.data() + beginningOffset, height, width };
		}

		utils::array2d_view<PixelColor> getCurrentLinesArray() const {
			return { pixelData.data() + beginningOffset, height, width };
		}

		void incrementStrip() {
			if (beginningOffset < maxOffset) {
				beginningOffset++;
				return;
			}

			std::copy(
				pixelData.data() + beginningOffset + 1,
				pixelData.data() + beginningOffset + width * height,
				pixelData.data()
			);
			beginningOffset = 0;
		}
	};

}
