/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/IntMixer.h"

namespace rxtd::audio_analyzer::image_utils {
	struct IntColor {
		struct Components {
			// order BGRA is very important for performance reasons
			// it's the standard order for BMP files
			// any other order makes image parsing extremely slow
			uint8_t b;
			uint8_t g;
			uint8_t r;
			uint8_t a;
		};

		union {
			Components rgba;
			uint32_t full;
		} value;

		template<typename MixType, uint8_t precision>
		[[nodiscard]]
		IntColor mixWith(IntColor other, IntMixer<MixType, precision> mixer) const {
			IntColor result{};
			result.value.rgba.a = static_cast<uint8_t>(mixer.mix(value.rgba.a, other.value.rgba.a));
			result.value.rgba.r = static_cast<uint8_t>(mixer.mix(value.rgba.r, other.value.rgba.r));
			result.value.rgba.g = static_cast<uint8_t>(mixer.mix(value.rgba.g, other.value.rgba.g));
			result.value.rgba.b = static_cast<uint8_t>(mixer.mix(value.rgba.b, other.value.rgba.b));
			return result;
		}

		[[nodiscard]]
		IntColor withR(index value) const {
			IntColor result = *this;
			result.value.rgba.r = static_cast<uint8_t>(value);
			return result;
		}

		[[nodiscard]]
		IntColor withG(index value) const {
			IntColor result = *this;
			result.value.rgba.g = static_cast<uint8_t>(value);
			return result;
		}

		[[nodiscard]]
		IntColor withB(index value) const {
			IntColor result = *this;
			result.value.rgba.b = static_cast<uint8_t>(value);
			return result;
		}

		[[nodiscard]]
		IntColor withA(index value) const {
			IntColor result = *this;
			result.value.rgba.a = static_cast<uint8_t>(value);
			return result;
		}

		friend bool operator==(const IntColor& lhs, const IntColor& rhs) {
			return lhs.value.full == rhs.value.full;
		}

		friend bool operator!=(const IntColor& lhs, const IntColor& rhs) {
			return !(lhs == rhs);
		}
	};
}
