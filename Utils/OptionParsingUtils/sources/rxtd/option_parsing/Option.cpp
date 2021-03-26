// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "Option.h"

#include "OptionList.h"
#include "OptionMap.h"
#include "OptionSequence.h"
#include "Tokenizer.h"

using rxtd::option_parsing::Option;
using rxtd::option_parsing::GhostOption;
using rxtd::option_parsing::OptionMap;
using rxtd::option_parsing::OptionList;
using rxtd::option_parsing::OptionSequence;
using StringUtils = rxtd::std_fixes::StringUtils;

rxtd::sview Option::asString(sview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

rxtd::isview Option::asIString(isview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

std::pair<GhostOption, GhostOption> Option::breakFirst(wchar_t separator) const & {
	sview view = getView();

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { GhostOption{ view }, {} };
	}

	auto first = GhostOption{ StringUtils::trim(sview(view.data(), delimiterPlace)) };
	auto rest = GhostOption{ StringUtils::trim(sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1)) };
	return { first, rest };
}

std::pair<Option, Option> Option::breakFirst(wchar_t separator) && {
	auto result = breakFirst(separator);
	if (isOwningSource()) {
		result.first.own();
		result.second.own();
	}
	return result;
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const & {
	return { getView(), optionDelimiter, nameDelimiter };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) && {
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), optionDelimiter, nameDelimiter };
	} else {
		return { getView(), optionDelimiter, nameDelimiter };
	}
}

OptionList Option::asList(wchar_t delimiter) const & {
	return { getView(), Tokenizer::parse(getView(), delimiter) };
}

OptionList Option::asList(wchar_t delimiter) && {
	auto list = Tokenizer::parse(getView(), delimiter);
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), std::move(list) };
	} else {
		return { getView(), std::move(list) };
	}
}

OptionList Option::asList(sview delimiter) const & {
	return { getView(), Tokenizer::parse(getView(), delimiter) };
}
OptionList Option::asList(sview delimiter) && {
	auto list = Tokenizer::parse(getView(), delimiter);
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), std::move(list) };
	} else {
		return { getView(), std::move(list) };
	}
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter,
	bool allowPostfix,
	const Logger& cl
) const & {
	return { getView(), optionBegin, optionEnd, optionDelimiter, allowPostfix, cl };
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter,
	bool allowPostfix,
	const Logger& cl
) && {
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), optionBegin, optionEnd, optionDelimiter, allowPostfix, cl };
	} else {
		return { getView(), optionBegin, optionEnd, optionDelimiter, allowPostfix, cl };
	}
}

std::wostream& rxtd::option_parsing::operator<<(std::wostream& stream, const Option& opt) {
	stream << opt.asString();
	return stream;
}
