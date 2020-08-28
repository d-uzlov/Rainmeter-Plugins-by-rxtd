/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Vector2D.h"
#include "IntColor.h"

namespace rxtd::utils {
	class BmpWriter {
	public:
		static void writeFile(const string& filepath, array2d_view<IntColor> imageData);
	};
}
