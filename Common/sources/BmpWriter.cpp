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

void utils::BmpWriter::writeFile(const string& filepath, const uint32_t* data, index width, index height) {
	BMPHeader header(static_cast<uint32_t>(height), static_cast<uint32_t>(width));

	FileWrapper file(filepath.c_str());

	file.write(reinterpret_cast<std::byte*>(&header), sizeof(header));
	file.write(reinterpret_cast<const std::byte*>(data), header.dibHeader.bitmapSizeInBytes);
}
