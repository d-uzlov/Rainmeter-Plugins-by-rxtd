/* 
 * Copyright (C) 2019 rxtd
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
		MatchTestRecord(sview pattern, bool substring);

		bool match(sview string) const;
	};

	class BlacklistManager {
		class MatchList {
			string source;
			std::vector<MatchTestRecord> list;

		public:
			MatchList() = default;

			MatchList(string sourceString, bool upperCase);

			bool match(sview view) const;

			bool empty() const;
		};

		MatchList blacklist;
		MatchList blacklistOrig;
		MatchList whitelist;
		MatchList whitelistOrig;

	public:
		void setLists(string black, string blackOrig, string white, string whiteOrig);

		bool isAllowed(sview searchName, sview originalName) const;
	};
}
