/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"
#include "array2d_view.h"
#include "IntColor.h"

namespace rxtd::utils {
	class BmpWriter {
#pragma pack( push, 1 )
		struct BMPHeader {
			static constexpr index dibSize = 108;

			struct {
				uint16_t id = 0x4d42; // == 'BM' (little-endian)
				uint32_t fileSizeInBytes{ };
				uint16_t reserved1 = 0;
				uint16_t reserved2 = 0;
				uint32_t pixelArrayOffsetInBytes{ };
			} fileHeader{ };

			struct {
				uint32_t headerSizeInBytes = dibSize;
				uint32_t bitmapWidthInPixels{ };
				uint32_t bitmapHeightInPixels{ };
				uint16_t colorPlaneCount = 1;
				uint16_t bitsPerPixel = 32;
				uint32_t compressionMethod = 0x03; // BIT_FIELDS
				uint32_t bitmapSizeInBytes{ };
				int32_t horizontalResolutionInPixelsPerMeter = 2835; // 72 ppi
				int32_t verticalResolutionInPixelsPerMeter = 2835; // 72 ppi
				uint32_t paletteColorCount = 0;
				uint32_t importantColorCount = 0;

				struct {
					uint32_t red = IntColor{ }.withR(0xFF).full;
					uint32_t green = IntColor { }.withG(0xFF).full;
					uint32_t blue = IntColor { }.withB(0xFF).full;
					uint32_t alpha = IntColor { }.withA(0xFF).full;
				} bitMask;
			} dibHeader{ };

		private:
			std::byte padding[dibSize - sizeof(dibHeader)]{ };

		public:
			BMPHeader(index width, index height) {
				fileHeader.pixelArrayOffsetInBytes = sizeof(fileHeader) + dibHeader.headerSizeInBytes;
				dibHeader.bitmapSizeInBytes = uint32_t(width * height * sizeof(uint32_t));
				fileHeader.fileSizeInBytes = fileHeader.pixelArrayOffsetInBytes + dibHeader.bitmapSizeInBytes;
				dibHeader.bitmapWidthInPixels = uint32_t(width);
				dibHeader.bitmapHeightInPixels = uint32_t(height);
			}
		};
#pragma pack( pop )

	public:
		static void writeFile(const string& filepath, array2d_view<uint32_t> imageData);
	};
}
