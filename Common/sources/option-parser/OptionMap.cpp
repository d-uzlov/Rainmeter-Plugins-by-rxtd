/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionMap.h"
#include "Tokenizer.h"

using namespace utils;

OptionMap::OptionMap(sview view, std::vector<wchar_t> &&source, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	AbstractOption(view, std::move(source)),
	paramsInfo(std::move(paramsInfo)) {
	fillParams();
}

GhostOption OptionMap::getUntouched(sview name) const & {
	const auto optionInfoPtr = find(name % ciView());
	if (optionInfoPtr == nullptr) {
		return { };
	}

	return GhostOption { optionInfoPtr->substringInfo.makeView(getView()) };
}

Option OptionMap::getUntouched(sview name) const && {
	const auto optionInfoPtr = find(name % ciView());
	if (optionInfoPtr == nullptr) {
		return { };
	}

	return Option{ optionInfoPtr->substringInfo.makeView(getView()) };
}

GhostOption OptionMap::get(isview name) const & {
	const auto optionInfoPtr = find(name);
	if (optionInfoPtr == nullptr) {
		return { };
	}

	optionInfoPtr->touched = true;
	return GhostOption { optionInfoPtr->substringInfo.makeView(getView()) };
}

bool OptionMap::has(isview name) const {
	const auto optionInfoPtr = find(name);
	return optionInfoPtr != nullptr;
}

std::vector<isview> OptionMap::getListOfUntouched() const {
	std::vector<isview> result;

	for (auto [name, valueInfo] : params) {
		if (valueInfo.touched) {
			continue;
		}

		result.emplace_back(name);
	}

	return result;
}

void OptionMap::fillParams() const {
	params.clear();
	for (auto [nameInfo, valueInfo] : paramsInfo) {
		params[nameInfo.makeView(getView()) % ciView()] = valueInfo;
	}
}

OptionMap::MapOptionInfo* OptionMap::find(isview name) const {
	if (params.empty() && !paramsInfo.empty()) {
		fillParams();
	}
	const auto iter = params.find(name);
	if (iter == params.end()) {
		return nullptr;
	}
	return &iter->second;
}
