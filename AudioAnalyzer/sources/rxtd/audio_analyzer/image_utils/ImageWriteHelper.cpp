// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "ImageWriteHelper.h"

#include <filesystem>
#include <fstream>

using rxtd::audio_analyzer::image_utils::ImageWriteHelper;
using rxtd::std_fixes::array2d_view;

void ImageWriteHelper::write(array2d_view<IntColor> pixels, bool empty, sview filepath) {
	if (state == State::eEMPTY && empty) {
		return;
	}

	std::filesystem::path directory{ std::wstring_view{ filepath } };
	directory.remove_filename();
	std::error_code ec;
	create_directories(directory, ec);
	if (ec) {
		// Something went wrong.
		// It's unlikely we can fix it.
		return;
	}

	std::ofstream fileStream(std::wstring_view{ filepath }, std::ios::binary);

	if (!fileStream.is_open()) {
		return;
	}

	BmpWriter::writeFile(fileStream, pixels);

	state = empty ? State::eEMPTY : State::eNOT_EMPTY;
}
