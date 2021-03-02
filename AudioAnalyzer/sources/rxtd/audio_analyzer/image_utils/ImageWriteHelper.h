/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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
		void write(std_fixes::array2d_view<IntColor> pixels, bool empty, const string& filepath);
	};
}
