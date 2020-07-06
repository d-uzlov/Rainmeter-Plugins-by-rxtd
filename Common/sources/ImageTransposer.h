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
#include "Color.h"

namespace rxtd::utils {
	class ImageTransposer {
		Vector2D<uint32_t> buffer;

	public:
		void transposeToBuffer(const Vector2D<Color>& imageData, index lineOffset);

		const Vector2D<uint32_t>& getBuffer() const;

	private:
		void transposeLine(index lineIndex, array_view<Color> lineData);
	};
}