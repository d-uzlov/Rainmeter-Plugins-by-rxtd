// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "MatchPattern.h"

namespace rxtd::perfmon {
	class BlacklistManager {
		class MatchList {
			string source;
			std::vector<MatchPattern> list;

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
