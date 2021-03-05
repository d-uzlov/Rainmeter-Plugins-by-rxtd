// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::perfmon {
	/// <summary>
	/// Class MatchPattern is a primitive matching helper.
	/// When pattern has a form of "*<values>*" (asterisks at the beginning and at the end)
	///		then MatchPattern checks if pattern is a substring of tested string.
	///	Otherwise checks the exact match.
	///
	///	The class doesn't manage it's memory, so the caller must ensure
	///	that source of the string_view lives at least as long as MatchPattern instance is used.
	/// </summary>
	class MatchPattern {
		sview substring{};
		bool matchSubstring{};

	public:
		MatchPattern() = default;

		explicit MatchPattern(sview pattern) {
			substring = pattern;
			matchSubstring = false;

			if (substring.length() > 2 && substring.front() == L'*' && substring.back() == L'*') {
				substring.remove_prefix(1);
				substring.remove_suffix(1);
				matchSubstring = true;
			}
		}

		[[nodiscard]]
		bool isEmpty() const {
			return substring.empty();
		}

		[[nodiscard]]
		bool match(sview string) const {
			if (!matchSubstring) {
				return substring == string;
			}
			return string.find(substring) != sview::npos;
		}

		friend bool operator<(const MatchPattern& lhs, const MatchPattern& rhs) {
			return lhs.substring < rhs.substring
				&& lhs.matchSubstring < rhs.matchSubstring;
		}

		[[nodiscard]]
		sview getName() const {
			return substring;
		}
	};
}
