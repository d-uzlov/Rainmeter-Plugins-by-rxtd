/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "IntMixer.h"

namespace rxtd::utils {
	union IntColor {
		struct {
			// order BGRA is very important for performance reasons
			// it's a standard order for BMP files
			// any other order makes image parsing extremely slow
			uint8_t b;
			uint8_t g;
			uint8_t r;
			uint8_t a;
		};

		uint32_t full;

		template<typename MixType, uint8_t precision>
		[[nodiscard]]
		IntColor mixWith(IntColor other, IntMixer<MixType, precision> mixer) const {
			IntColor result{};
			result.a = mixer.mix(a, other.a);
			result.r = mixer.mix(r, other.r);
			result.g = mixer.mix(g, other.g);
			result.b = mixer.mix(b, other.b);
			return result;
		}

		[[nodiscard]]
		IntColor withR(index value) const {
			IntColor result = *this;
			result.r = uint8_t(value);
			return result;
		}

		[[nodiscard]]
		IntColor withG(index value) const {
			IntColor result = *this;
			result.g = uint8_t(value);
			return result;
		}

		[[nodiscard]]
		IntColor withB(index value) const {
			IntColor result = *this;
			result.b = uint8_t(value);
			return result;
		}

		[[nodiscard]]
		IntColor withA(index value) const {
			IntColor result = *this;
			result.a = uint8_t(value);
			return result;
		}

		friend bool operator==(const IntColor& lhs, const IntColor& rhs) {
			return lhs.full == rhs.full;
		}

		friend bool operator!=(const IntColor& lhs, const IntColor& rhs) {
			return !(lhs == rhs);
		}
	};
}
