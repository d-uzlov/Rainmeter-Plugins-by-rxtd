/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::perfmon {
	struct MatchTestRecord {
		sview pattern;
		bool matchSubstring{};

		MatchTestRecord() = default;

		MatchTestRecord(sview pattern, bool substring) :
			pattern(pattern),
			matchSubstring(substring) { }

		[[nodiscard]]
		bool match(sview string) const {
			if (!matchSubstring) {
				return pattern == string;
			}
			return string.find(pattern) != sview::npos;
		}
	};

	class BlacklistManager {
		class MatchList {
			string source;
			std::vector<MatchTestRecord> list;

		public:
			MatchList() = default;

			MatchList(string sourceString, bool upperCase);

			[[nodiscard]]
			bool match(sview view) const {
				return std::any_of(
					list.begin(), list.end(), [view](auto record) {
						return record.match(view);
					}
				);
			}

			[[nodiscard]]
			bool empty() const{
				return list.empty();
			}
		};

		MatchList blacklist;
		MatchList blacklistOrig;
		MatchList whitelist;
		MatchList whitelistOrig;

	public:
		void setLists(string black, string blackOrig, string white, string whiteOrig) {
			blacklist = { std::move(black), true };
			blacklistOrig = { std::move(blackOrig), false };
			whitelist = { std::move(white), true };
			whitelistOrig = { std::move(whiteOrig), false };
		}

		[[nodiscard]]
		bool isAllowed(sview searchName, sview originalName) const;
	};
}
