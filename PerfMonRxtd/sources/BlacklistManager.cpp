/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlacklistManager.h"
#include "Windows.h"

#include "undef.h"

using namespace perfmon;

MatchTestRecord::MatchTestRecord() = default;

MatchTestRecord::MatchTestRecord(sview pattern, bool substring) :
	pattern(pattern),
	matchSubstring(substring) { }

bool MatchTestRecord::match(sview string) const {
	if (!matchSubstring) {
		return pattern == string;
	}
	return string.find(pattern) != sview::npos;
}

BlacklistManager::MatchList::MatchList() = default;

BlacklistManager::MatchList::MatchList(rxtd::utils::OptionParser::OptionList optionList, bool upperCase) {
	auto[optVec, optList] = std::move(optionList).consume();
	source = std::move(optVec);
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

bool BlacklistManager::MatchList::match(sview view) const {
	for (auto record : list) {
		if (record.match(view)) {
			return true;
		}
	}

	return false;
}

bool BlacklistManager::MatchList::empty() const {
	return list.empty();
}

void BlacklistManager::setLists(utils::OptionParser::OptionList black, utils::OptionParser::OptionList blackOrig,
	utils::OptionParser::OptionList white, utils::OptionParser::OptionList whiteOrig) {
	blacklist = { std::move(black), true };
	blacklistOrig = { std::move(blackOrig), false };
	whitelist = { std::move(white), true };
	whitelistOrig = { std::move(whiteOrig), false };
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
