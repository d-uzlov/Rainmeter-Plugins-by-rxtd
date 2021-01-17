/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlacklistManager.h"

#include "my-windows.h"
#include "option-parser/Option.h"
#include "option-parser/OptionList.h"

using namespace perfmon;

BlacklistManager::MatchList::MatchList(string sourceString, bool upperCase) {
	auto [_, optList] = utils::Option{ sourceString }.asList(L'|').consume();

	source = std::move(sourceString);
	if (upperCase) {
		CharUpperW(source.data());
	}

	list.reserve(optList.size());

	for (auto viewInfo : optList) {
		auto view = viewInfo.makeView(source);

		if (view.length() < 3) {
			list.emplace_back(view, false);
			continue;
		}

		if (view.front() == L'*' && view.back() == L'*') {
			list.emplace_back(view.substr(1, view.length() - 2), true);
			continue;
		}

		list.emplace_back(view, false);
	}
}

bool BlacklistManager::isAllowed(sview searchName, sview originalName) const {
	// blacklist → discard
	if (blacklist.match(searchName) || blacklistOrig.match(originalName)) {
		return false;
	}

	// no whitelists → OK
	if (whitelist.empty() && whitelistOrig.empty()) {
		return true;
	}

	// at least one whitelist specified, need to match at least one of them
	return whitelist.match(searchName) || whitelistOrig.match(originalName);
}
