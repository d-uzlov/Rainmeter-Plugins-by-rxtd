/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionMap.h"
#include "StringUtils.h"
#include "Tokenizer.h"

using namespace common::options;

OptionMap::OptionMap(sview view, wchar_t optionDelimiter, wchar_t nameDelimiter) :
	OptionBase(view), optionDelimiter(optionDelimiter), nameDelimiter(nameDelimiter) {
	parseParams();
}

OptionMap::OptionMap(std::vector<wchar_t>&& source, wchar_t optionDelimiter, wchar_t nameDelimiter) :
	OptionBase(std::move(source)), optionDelimiter(optionDelimiter), nameDelimiter(nameDelimiter) {
	parseParams();
}

GhostOption OptionMap::get(isview name) const & {
	const auto optionInfoPtr = find(name);
	if (optionInfoPtr == nullptr) {
		return {};
	}

	optionInfoPtr->touched = true;
	return GhostOption{ optionInfoPtr->view };
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

void OptionMap::parseParams() {
	auto source = getView();
	auto list = Tokenizer::parse(source, optionDelimiter);

	for (const auto& viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(source).find_first_of(nameDelimiter);
		if (delimiterPlace == sview::npos) {
			params[viewInfo.makeView(source) % ciView()] = {};
			continue;
		}

		auto name = utils::StringUtils::trimInfo(source, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = utils::StringUtils::trimInfo(source, viewInfo.substr(delimiterPlace + 1));

		params[name.makeView(source) % ciView()] = { value.makeView(source) };
	}
}

const OptionMap::MapOptionInfo* OptionMap::find(isview name) const {
	const auto iter = params.find(name);
	if (iter == params.end()) {
		return nullptr;
	}
	return &iter->second;
}
