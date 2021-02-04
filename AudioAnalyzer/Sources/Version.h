/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::audio_analyzer {
	class Version {
	public:
		enum Value {
			eVERSION1 = 1,
			eVERSION2 = 2,
		};

		Value value = eVERSION1;

		Version() = default;

		explicit Version(Value value) : value(value) {}

		/// Transforms MagicNumber into api version
		/// Throws std::runtime_error when number doesn't correspond to any version
		[[nodiscard]]
		static Version parseVersion(index magicNumber) {
			switch (magicNumber) {
			case 0: return Version{ eVERSION1 };
			case 104: return Version{ eVERSION2 };
			default: throw std::runtime_error{ "invalid magic number" };
			}
		}

		friend bool operator==(const Version& lhs, const Version& rhs) {
			return lhs.value == rhs.value;
		}

		friend bool operator!=(const Version& lhs, const Version& rhs) {
			return !(lhs == rhs);
		}

		[[nodiscard]]
		bool operator<(Version other) const {
			return value < other.value;
		}

		[[nodiscard]]
		bool operator<(Value val) const {
			return value < val;
		}
	};
}
