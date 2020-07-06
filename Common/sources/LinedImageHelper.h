/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Vector2D.h"

namespace rxtd::utils {
	class LinedImageHelper {
		Vector2D<uint32_t> buffer;
		index lastLineIndex = 0;
		mutable std::vector<uint32_t> writeBuffer;
		uint32_t backgroundValue = { };
		uint32_t lastFillValue = { };
		index sameLinesCount = 0;

	public:
		void setBackground(uint32_t value);

		void setImageWidth(index width);

		void setImageHeight(index height);

		array_span<uint32_t> nextLine();

		void fillNextLine(uint32_t value);

		void writeTransposed(const string& filepath) const;

	private:
		array_span<uint32_t> nextLineNonBreaking();
	};
}
