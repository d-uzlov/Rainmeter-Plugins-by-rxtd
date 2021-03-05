// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "IntColor.h"
#include "rxtd/std_fixes/Vector2D.h"

namespace rxtd::audio_analyzer::image_utils {
	class BmpWriter {
	public:
		static void writeFile(std::ostream& stream, std_fixes::array2d_view<IntColor> imageData);
	};
}
