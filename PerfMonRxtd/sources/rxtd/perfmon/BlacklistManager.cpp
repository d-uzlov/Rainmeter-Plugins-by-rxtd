// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "BlacklistManager.h"

#include "rxtd/option_parsing/Tokenizer.h"

using rxtd::perfmon::BlacklistManager;
using rxtd::std_fixes::StringUtils;
using rxtd::option_parsing::Tokenizer;

BlacklistManager::MatchList::MatchList(string sourceString, bool upperCase) {
	source = std::move(sourceString);
	if (upperCase) {
		StringUtils::makeUppercaseInPlace(source);
	}

	auto tokens = Tokenizer::parse(source, L'|');

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
