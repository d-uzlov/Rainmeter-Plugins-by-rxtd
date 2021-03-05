// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

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
