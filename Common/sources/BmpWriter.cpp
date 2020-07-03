/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BmpWriter.h"
#include "windows-wrappers/FileWrapper.h"

#include "undef.h"

void utils::BmpWriter::writeFile(const string& filepath, const uint32_t* data, index width, index height, index offset,
	array_span<uint32_t> writeBuffer) {
	const auto pixelsCount = width * height;
	if (pixelsCount <= 0) {
		return;
	}
	if (writeBuffer.size() < pixelsCount) {
		throw std::exception { };
	}

	class TwoDimensionalArrayIndexer {
		const index width;

	public:
		explicit TwoDimensionalArrayIndexer(index width) : width(width) { }

		index toPlain(index x, index y) const {
			return y * width + x;
		}
	};

	TwoDimensionalArrayIndexer si(width);
	TwoDimensionalArrayIndexer di(height);

	for (index y = offset; y < height; y++) {
		for (index x = 0; x < width; x++) {
			const auto sourceIndex = si.toPlain(x, y);
			const auto outputIndex = di.toPlain(y - offset, x);
			writeBuffer[outputIndex] = data[sourceIndex];
		}
	}

	for (index y = 0; y < offset; y++) {
		for (index x = 0; x < width; x++) {
			const auto sourceIndex = si.toPlain(x, y);
			const auto outputIndex = di.toPlain(y + height - offset, x);
			writeBuffer[outputIndex] = data[sourceIndex];
		}
	}

	BMPHeader header(static_cast<uint32_t>(height), static_cast<uint32_t>(width));

	FileWrapper file(filepath.c_str());

	file.write(reinterpret_cast<std::byte*>(&header), sizeof(header));
	file.write(reinterpret_cast<std::byte*>(writeBuffer.data()), header.dibHeader.bitmapSizeInBytes);
}
