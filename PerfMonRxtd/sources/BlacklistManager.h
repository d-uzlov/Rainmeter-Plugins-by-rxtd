/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <vector>
#include "OptionParser.h"

#undef min
#undef max

namespace rxpm {
	struct MatchTestRecord {
		std::wstring_view pattern;
		bool matchSubstring { };

		MatchTestRecord();
		MatchTestRecord(std::wstring_view pattern, bool substring);

		bool match(std::wstring_view string) const;
	};

	class BlacklistManager {
		class MatchList {
			std::vector<wchar_t> source;
			std::vector<MatchTestRecord> list;

		public:
			MatchList();

			MatchList(rxu::OptionParser::OptionList optionList, bool upperCase);

			bool match(std::wstring_view string) const;

			bool empty() const;
		};

		MatchList blacklist;
		MatchList blacklistOrig;
		MatchList whitelist;
		MatchList whitelistOrig;

	public:
		void setLists(
			rxu::OptionParser::OptionList black,
			rxu::OptionParser::OptionList blackOrig,
			rxu::OptionParser::OptionList white,
			rxu::OptionParser::OptionList whiteOrig
		);

		bool isAllowed(std::wstring_view searchName, std::wstring_view originalName) const;
	};
}
