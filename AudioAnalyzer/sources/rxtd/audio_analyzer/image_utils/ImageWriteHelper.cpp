/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ImageWriteHelper.h"

#include <filesystem>
#include <fstream>

using rxtd::audio_analyzer::image_utils::ImageWriteHelper;
using rxtd::std_fixes::array2d_view;

void ImageWriteHelper::write(array2d_view<IntColor> pixels, bool empty, const string& filepath) {
	if (emptinessWritten && empty) {
		return;
	}

	emptinessWritten = false;

	std::filesystem::path directory{ filepath };
	directory.remove_filename();
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);
	if (ec) {
		// Something went wrong.
		// It's unlikely we can fix it.
		return;
	}

	std::ofstream fileStream(filepath, std::ios::binary);

	if (!fileStream.is_open()) {
		return;
	}

	BmpWriter::writeFile(fileStream, pixels);

	emptinessWritten = empty;
}
