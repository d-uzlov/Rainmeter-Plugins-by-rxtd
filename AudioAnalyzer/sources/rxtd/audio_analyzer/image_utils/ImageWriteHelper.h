// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "BmpWriter.h"

namespace rxtd::audio_analyzer::image_utils {
	class ImageWriteHelper {
		enum class State {
			eUNINITIALIZED,
			eEMPTY,
			eNOT_EMPTY,
		};
		State state = State::eUNINITIALIZED;

	public:
		void write(std_fixes::array2d_view<IntColor> pixels, bool empty, sview filepath);
	};
}
