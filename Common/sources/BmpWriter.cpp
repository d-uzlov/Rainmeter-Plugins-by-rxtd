/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BmpWriter.h"
#include "windows-wrappers/FileWrapper.h"
#include <string>

#pragma warning(disable : 4267)

void rxu::BmpWriter::writeFile(const std::wstring &filepath, const uint32_t* data, size_t width, size_t height, size_t offset, uint32_t* writeBuffer, size_t bufferSize) {
	const auto pixelsCount = width * height;
	if (pixelsCount <= 0) {
		return;
	}
	if (bufferSize < pixelsCount) {
		std::terminate();
	}

	class TwoDimensionalArrayIndexer {
		const size_t width;

	public:
		explicit TwoDimensionalArrayIndexer(size_t width) : width(width) { }

		size_t toPlain(size_t x, size_t y) const {
			return y * width + x;
		}
	};

	TwoDimensionalArrayIndexer si(width);
	TwoDimensionalArrayIndexer di(height);

	for (unsigned y = offset; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			const auto sourceIndex = si.toPlain(x, y);
			const auto outputIndex = di.toPlain(y - offset, x);
			writeBuffer[outputIndex] = data[sourceIndex];
		}
	}

	for (unsigned y = 0; y < offset; y++) {
		for (unsigned x = 0; x < width; x++) {
			const auto sourceIndex = si.toPlain(x, y);
			const auto outputIndex = di.toPlain(y + height - offset, x);
			writeBuffer[outputIndex] = data[sourceIndex];
		}
	}

	BMPHeader header(height, width);

	FileWrapper file(filepath.c_str());

	file.write(reinterpret_cast<uint8_t*>(&header), sizeof(header));
	file.write(reinterpret_cast<uint8_t*>(writeBuffer), header.dibHeader.bitmapSizeInBytes);
}
