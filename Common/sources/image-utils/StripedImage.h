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
#include "GrowingVector.h"

namespace rxtd::utils {
	template <typename PIXEL_VALUE_TYPE>
	class StripedImage {
	public:
		using PixelValueType = PIXEL_VALUE_TYPE;

	protected:
		GrowingVector<PixelValueType> pixelData{ };
		index width = 0;
		index height = 0;

		PixelValueType backgroundValue = { };
		PixelValueType lastFillValue = { };
		index sameStripsCount = 0;
		bool stationary = false;
		index stationaryOffset = 0;

	public:
		// Must be called before #setDimensions
		void setBackground(PixelValueType value) {
			backgroundValue = value;
		}

		void setStationary(bool value) {
			stationary = value;
		}

		void setDimensions(index _width, index _height) {
			if (width == _width && height == _height) {
				return;
			}

			this->width = _width;
			this->height = _height;

			const index imagePixelsCount = _width * _height;
			const index maxOffset = getReserveSize(imagePixelsCount);

			pixelData.reset(imagePixelsCount, backgroundValue);
			pixelData.setMaxSize(imagePixelsCount + maxOffset);

			lastFillValue = backgroundValue;
			sameStripsCount = _width - 1;
		}

		void pushStrip(array_view<PixelValueType> stripData) {
			sameStripsCount = 0;

			index nextStripIndex = incrementAndGetIndex();
			auto imageLines = getCurrentLinesArray(); // must be after #incrementAndGetIndex
			for (index i = 0; i < stripData.size(); i++) {
				imageLines[i][nextStripIndex] = stripData[i];
			}
		}

		void pushEmptyStrip(PixelValueType value) {
			if (lastFillValue != value) {
				lastFillValue = value;
				sameStripsCount = 1;
			} else if (isEmpty()) {
				if (!isForced()) {
					return;
				}
			} else {
				sameStripsCount++;
			}

			index nextStripIndex = incrementAndGetIndex();
			auto imageLines = getCurrentLinesArray(); // must be after #incrementAndGetIndex
			for (index i = 0; i < height; i++) {
				imageLines[i][nextStripIndex] = value;
			}
		}

		void correctLastLine(index pixelIndex, PixelValueType value) {
			auto lastStripIndex = getLastStripIndex();
			auto imageLines = getCurrentLinesArray();
			auto line = imageLines[pixelIndex];
			line[lastStripIndex] = value;
		}

		[[nodiscard]]
		bool isEmpty() const {
			return sameStripsCount >= width;
		}

		[[nodiscard]]
		bool isForced() const {
			// should be used to ensure
			// that when image is stationary it will be cleared fully
			// instead of stopping in the middle

			const index lastStripIndex = getLastStripIndex();
			const bool forced = lastStripIndex != width - 1;
			return forced || !isEmpty();
		}

		[[nodiscard]]
		array2d_view<PixelValueType> getPixels() const {
			return getCurrentLinesArray();
		}

		[[nodiscard]]
		array2d_span<PixelValueType> getPixelsWritable() {
			return getCurrentLinesArray();
		}

		[[nodiscard]]
		index getLastStripIndex() const {
			if (!stationary) {
				return width - 1;
			}

			index offset = stationaryOffset - 1;
			if (offset < 0) {
				offset += width;
			}
			return offset;
		}

		[[nodiscard]]
		index getPastLastStripIndex() const {
			if (!stationary) {
				return 0;
			}

			return stationaryOffset;
		}

	private:
		// returns index of next string to write to
		index incrementAndGetIndex() {
			if (stationary) {
				const index resultIndex = stationaryOffset;
				incrementStationary();
				return resultIndex;
			}

			incrementStrip();
			return width - 1;
		}

		[[nodiscard]]
		index getReserveSize(index size) const {
			if (stationary) {
				return 0;
			}

			constexpr double reserveCoef = 0.5;
			return static_cast<index>(std::ceil(size * reserveCoef));
		}

		[[nodiscard]]
		array2d_span<PixelValueType> getCurrentLinesArray() {
			return { pixelData.getPointer(), height, width };
		}

		[[nodiscard]]
		array2d_view<PixelValueType> getCurrentLinesArray() const {
			return { pixelData.getPointer(), height, width };
		}

		void incrementStrip() {
			pixelData.removeFirst(1);
			pixelData.allocateNext(1);
		}

		void incrementStationary() {
			stationaryOffset++;

			if (stationaryOffset >= width) {
				stationaryOffset = 0;
			}
		}
	};
}
