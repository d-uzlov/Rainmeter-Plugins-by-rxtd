// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "rxtd/GrowingVector.h"
#include "rxtd/std_fixes/Vector2D.h"

namespace rxtd::audio_analyzer::image_utils {
	template<typename PixelValueT>
	class StripedImage {
		using PixelValueType = PixelValueT;

		GrowingVector<PixelValueType> pixelData{};
		index width = 0;
		index height = 0;

		PixelValueType backgroundValue = {};
		PixelValueType lastFillValue = {};
		index sameStripsCount = 0;
		bool stationary = false;
		index stationaryOffset = 0;

	public:
		void setParams(index _width, index _height, PixelValueType _backgroundValue, bool _stationary) {
			if (width == _width
				&& height == _height
				&& backgroundValue == _backgroundValue
				&& stationary == _stationary
			) {
				return;
			}

			backgroundValue = _backgroundValue;
			stationary = _stationary;
			width = _width;
			height = _height;

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
			if (lastFillValue != value || sameStripsCount == 0) {
				lastFillValue = value;
				sameStripsCount = 1;
			} else if (isEmpty()) {
				return;
			} else {
				sameStripsCount++;
			}

			index nextStripIndex = incrementAndGetIndex();
			auto imageLines = getCurrentLinesArray(); // must be after #incrementAndGetIndex
			for (index i = 0; i < height; i++) {
				imageLines[i][nextStripIndex] = value;
			}
		}

		[[nodiscard]]
		bool isEmpty() const {
			const bool bufferIsEmpty = sameStripsCount >= width;
			if (!stationary) {
				return bufferIsEmpty;
			} else {
				index lastStripIndex = stationaryOffset - 1;
				if (lastStripIndex < 0) {
					lastStripIndex += width;
				}

				return bufferIsEmpty && lastStripIndex == width - 1;
			}
		}

		[[nodiscard]]
		std_fixes::array2d_view<PixelValueType> getPixels() const {
			return getCurrentLinesArray();
		}

		[[nodiscard]]
		std_fixes::array2d_span<PixelValueType> getPixelsWritable() {
			return getCurrentLinesArray();
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
			return static_cast<index>(std::ceil(static_cast<double>(size) * reserveCoef));
		}

		[[nodiscard]]
		std_fixes::array2d_span<PixelValueType> getCurrentLinesArray() {
			return { pixelData.getPointer(), height, width };
		}

		[[nodiscard]]
		std_fixes::array2d_view<PixelValueType> getCurrentLinesArray() const {
			return { pixelData.getPointer(), height, width };
		}

		void incrementStrip() {
			pixelData.removeFirst(1);
			(void)pixelData.allocateNext(1);
		}

		void incrementStationary() {
			stationaryOffset++;

			if (stationaryOffset >= width) {
				stationaryOffset = 0;
			}
		}
	};
}
