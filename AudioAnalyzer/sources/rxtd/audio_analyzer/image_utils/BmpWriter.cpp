/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BmpWriter.h"

using rxtd::audio_analyzer::image_utils::IntColor;
using rxtd::audio_analyzer::image_utils::BmpWriter;

#pragma pack( push, 1 )
struct BMPHeader {
	static constexpr rxtd::index dibSize = 108;

	struct {
		char id2[2] = { 'B', 'M' };
		uint32_t fileSizeInBytes{};
		uint16_t reserved1 = 0;
		uint16_t reserved2 = 0;
		uint32_t pixelArrayOffsetInBytes{};
	} fileHeader{};

	struct {
		uint32_t headerSizeInBytes = dibSize;
		uint32_t bitmapWidthInPixels{};
		uint32_t bitmapHeightInPixels{};
		uint16_t colorPlaneCount = 1;
		uint16_t bitsPerPixel = 32;
		uint32_t compressionMethod = 0x03; // BIT_FIELDS
		uint32_t bitmapSizeInBytes{};
		int32_t horizontalResolutionInPixelsPerMeter = 2835; // 72 ppi
		int32_t verticalResolutionInPixelsPerMeter = 2835; // 72 ppi
		uint32_t paletteColorCount = 0;
		uint32_t importantColorCount = 0;

		struct {
			uint32_t r = IntColor{}.withR(0xFF).value.full;
			uint32_t g = IntColor{}.withG(0xFF).value.full;
			uint32_t b = IntColor{}.withB(0xFF).value.full;
			uint32_t a = IntColor{}.withA(0xFF).value.full;
		} bitMask;
	} dibHeader{};

private:
	std::byte padding[dibSize - sizeof(dibHeader)]{}; // NOLINT(clang-diagnostic-unused-private-field)

public:
	BMPHeader(rxtd::index width, rxtd::index height) {
		fileHeader.pixelArrayOffsetInBytes = sizeof(fileHeader) + dibHeader.headerSizeInBytes;
		dibHeader.bitmapSizeInBytes = static_cast<uint32_t>(width * height * sizeof(uint32_t));
		fileHeader.fileSizeInBytes = fileHeader.pixelArrayOffsetInBytes + dibHeader.bitmapSizeInBytes;
		dibHeader.bitmapWidthInPixels = static_cast<uint32_t>(width);
		dibHeader.bitmapHeightInPixels = static_cast<uint32_t>(height);
	}
};
#pragma pack( pop )


void BmpWriter::writeFile(std::ostream& stream, std_fixes::array2d_view<IntColor> imageData) {
	BMPHeader header(imageData.getBufferSize(), imageData.getBuffersCount());

	stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
	stream.write(reinterpret_cast<const char*>(imageData[0].data()), header.dibHeader.bitmapSizeInBytes);

	// FileWrapper file(filepath.c_str());
	// file.write(&header, sizeof(header));
	// file.write(imageData[0].data(), header.dibHeader.bitmapSizeInBytes);
}
