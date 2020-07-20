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

void utils::BmpWriter::writeFile(const string& filepath, array2d_view<uint32_t> imageData) {
	BMPHeader header(imageData.getBufferSize(), imageData.getBuffersCount());

	FileWrapper file(filepath.c_str());

	file.write(&header, sizeof(header));
	file.write(imageData[0].data(), header.dibHeader.bitmapSizeInBytes);
}
