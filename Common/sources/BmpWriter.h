/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <cstdint>
#include <xstring>

namespace rxu {
	class BmpWriter {
#pragma pack( push, 1 ) 
		struct BMPHeader {
			static constexpr int dibSize = 108;

			struct {
				uint16_t ID = 0x4d42; // == 'BM' (little-endian)
				uint32_t fileSizeInBytes { };
				uint16_t reserved1 = 0;
				uint16_t reserved2 = 0;
				uint32_t pixelArrayOffsetInBytes { };
			} fileHeader { };

			struct {
				uint32_t headerSizeInBytes = dibSize;
				uint32_t bitmapWidthInPixels { };
				uint32_t bitmapHeightInPixels { };
				uint16_t colorPlaneCount = 1;
				uint16_t bitsPerPixel = 32;
				uint32_t compressionMethod = 0x03; // BIT_FIELDS
				uint32_t bitmapSizeInBytes { };
				int32_t horizontalResolutionInPixelsPerMeter = 2835; // 72 ppi
				int32_t verticalResolutionInPixelsPerMeter = 2835; // 72 ppi
				uint32_t paletteColorCount = 0;
				uint32_t importantColorCount = 0;

				struct {
					uint32_t red = 0x00FF0000u;
					uint32_t green = 0x0000FF00u;
					uint32_t blue = 0x000000FFu;
					uint32_t alpha = 0xFF000000u;
				} bitMask;
			} dibHeader { };

			BMPHeader(uint32_t width, uint32_t height) {
				fileHeader.pixelArrayOffsetInBytes = sizeof(fileHeader) + dibHeader.headerSizeInBytes;
				dibHeader.bitmapSizeInBytes = width * height * sizeof(uint32_t);
				fileHeader.fileSizeInBytes = fileHeader.pixelArrayOffsetInBytes + dibHeader.bitmapSizeInBytes;
				dibHeader.bitmapWidthInPixels = width;
				dibHeader.bitmapHeightInPixels = height;
			}

		private:
			uint8_t pad[dibSize - sizeof(dibHeader)] { };
		};
#pragma pack( pop )

	public:
		static void writeFile(const std::wstring& filepath, const uint32_t* data, size_t width, size_t height, size_t offset, uint32_t* writeBuffer, size_t
		                      bufferSize);
	};
}
