/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlacklistManager.h"

#include "option-parsing/Tokenizer.h"

using namespace rxtd::perfmon;

BlacklistManager::MatchList::MatchList(string sourceString, bool upperCase) {
	source = std::move(sourceString);
	if (upperCase) {
		utils::StringUtils::makeUppercaseInPlace(source);
	}

	auto tokens = common::options::Tokenizer::parse(source, L'|');

	list.reserve(tokens.size());

	for (auto viewInfo : tokens) {
		list.emplace_back(viewInfo.makeView(source));
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
