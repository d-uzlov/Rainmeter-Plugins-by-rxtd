/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "BmpWriter.h"

namespace rxtd::utils {
	class ImageWriteHelper {
		bool emptinessWritten = false;

	public:
		void write(array2d_view<uint32_t> pixels, bool empty, const string& filepath) {
			if (emptinessWritten && empty) {
				return;
			}

			BmpWriter::writeFile(filepath, pixels);

			emptinessWritten = empty;
		}
	};
}
