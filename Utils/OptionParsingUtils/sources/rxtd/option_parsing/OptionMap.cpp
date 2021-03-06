// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "OptionMap.h"
#include "Tokenizer.h"
#include "rxtd/std_fixes/StringUtils.h"

using rxtd::option_parsing::OptionMap;
using rxtd::option_parsing::GhostOption;
using rxtd::std_fixes::StringUtils;

OptionMap::OptionMap(sview view, wchar_t optionDelimiter, wchar_t nameDelimiter) :
	OptionBase(view), optionDelimiter(optionDelimiter), nameDelimiter(nameDelimiter) {
	parseParams();
}

OptionMap::OptionMap(SourceType&& source, wchar_t optionDelimiter, wchar_t nameDelimiter) :
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

std::vector<rxtd::isview> OptionMap::getListOfUntouched() const {
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

		auto name = StringUtils::trimInfo(source, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = StringUtils::trimInfo(source, viewInfo.substr(delimiterPlace + 1));

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
