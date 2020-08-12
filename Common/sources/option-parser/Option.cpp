/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Option.h"
#include "MathExpressionParser.h"
#include "Tokenizer.h"

#include "OptionMap.h"
#include "OptionList.h"
#include "OptionSequence.h"
#include "RainmeterWrappers.h"

using namespace utils;

sview Option::asString(sview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview Option::asIString(isview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

double Option::asFloat(double defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

float Option::asFloatF(float defaultValue) const {
	return float(asFloat(defaultValue));
}

bool Option::asBool(bool defaultValue) const {
	const isview view = getView() % ciView();
	if (view.empty()) {
		return defaultValue;
	}
	if (view == L"true") {
		return true;
	}
	if (view == L"false") {
		return false;
	}
	return asFloat() != 0.0;
}

OptionSeparated Option::breakFirst(wchar_t separator) const {
	sview view = getView();

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { *this, { } };
	}

	auto first = Option{ StringUtils::trim(sview(view.data(), delimiterPlace)) };
	auto rest = Option{ StringUtils::trim(sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1)) };
	return OptionSeparated{ first, rest };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const & {
	return { getView(), { }, parseMapParams(getView(), optionDelimiter, nameDelimiter) };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) && {
	// if this option owns a string, then view points to it, and .consumeSource() destroys it
	// so we need to everything we want with the view before calling .consumeSource()
	const sview view = getView();
	auto params = parseMapParams(view, optionDelimiter, nameDelimiter);
	return { view, std::move(*this).consumeSource(), std::move(params) };
}

OptionList Option::asList(wchar_t delimiter) const & {
	return { getView(), { }, Tokenizer::parse(getView(), delimiter) };
}

OptionList Option::asList(wchar_t delimiter) && {
	// if this option owns a string, then view points to it, and .consumeSource() destroys it
	// so we need to everything we want with the view before calling .consumeSource()
	const sview view = getView();
	auto list = Tokenizer::parse(view, delimiter);
	return { view, std::move(*this).consumeSource(), std::move(list) };
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) const & {
	return { getView(), { }, optionBegin, optionEnd, optionDelimiter };
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) && {
	// if this option owns a string, then view points to it, and .consumeSource() destroys it
	// so we need to everything we want with the view before calling .consumeSource()
	const sview view = getView();
	return { view, std::move(*this).consumeSource(), optionBegin, optionEnd, optionDelimiter };
}

double Option::parseNumber(sview source) {
	MathExpressionParser parser(source);

	parser.parse();

	if (parser.isError()) {
		BufferPrinter printer;
		printer.print(L"can't parse '{}' as a number", source);
		Rainmeter::sourcelessLog(printer.getBufferPtr());
		return 0;
	}

	auto exp = parser.getExpression();
	exp.solve();

	if (exp.type != ExpressionType::eNUMBER) {
		return 0;
	}

	return exp.number;
}

std::map<SubstringViewInfo, SubstringViewInfo> Option::parseMapParams(
	sview source,
	wchar_t optionDelimiter,
	wchar_t nameDelimiter
) {
	auto list = Tokenizer::parse(source, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo{ };
	for (const auto& viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(source).find_first_of(nameDelimiter);
		if (delimiterPlace == sview::npos) {
			// tokenizer.parse is guarantied to return non-empty views
			paramsInfo[viewInfo] = { };
			continue;
		}

		auto name = StringUtils::trimInfo(source, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = StringUtils::trimInfo(source, viewInfo.substr(delimiterPlace + 1));

		paramsInfo[name] = value;
	}

	return paramsInfo;
}
