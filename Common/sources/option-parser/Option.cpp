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

#include "undef.h"

using namespace utils;

Option::Option(sview view) : AbstractOption(view, { }) {
}

Option::Option(wchar_t* view) : Option(sview{view}) {
	own();
}

sview Option::asString(sview defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview Option::asIString(isview defaultValue) const {
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

Color Option::asColor(Color defaultValue) const {
	sview view = getView();

	if (view.empty()) {
		return defaultValue;
	}

	Tokenizer tokenizer;
	auto numbers = tokenizer.parse(view, L',');
	const auto count = index(numbers.size());
	if (count != 3 && count != 4) {
		return defaultValue;
	}

	float values[4];
	values[3] = 1.0;
	for (index i = 0; i < count; ++i) {
		values[i] = parseNumber(numbers[i].makeView(view));
	}

	return { values[0], values[1], values[2], values[3] };
}

OptionSeparated Option::breakFirst(wchar_t separator) const {
	sview view = getView();

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { *this, { } };
	}

	auto first = Option { sview(view.data(), delimiterPlace) };
	auto rest = Option { sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1) };
	return OptionSeparated { first, rest };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const & {
	return { getView(), { }, parseMapParams(getView(), optionDelimiter, nameDelimiter) };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) && {
	// consumeSource() below may destroy view content so we need to do everything before it
	const sview view = getView();
	auto params = parseMapParams(view, optionDelimiter, nameDelimiter);
	return { view, std::move(std::move(*this).consumeSource()), std::move(params) };
}

OptionList Option::asList(wchar_t delimiter) const & {
	return { getView(), { }, Tokenizer::parse(getView(), delimiter) };
}

OptionList Option::asList(wchar_t delimiter) && {
	// consumeSource() below may destroy view content so we need to do everything before it
	const sview view = getView();
	auto list = Tokenizer::parse(view, delimiter);
	return { view, std::move(std::move(*this).consumeSource()), std::move(list) };
}

OptionSequence Option::asSequence(wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) const & {
	return { getView(), { }, optionBegin, optionEnd, paramDelimiter, optionDelimiter };
}

OptionSequence Option::asSequence(wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) && {
	// consumeSource() below may destroy view content so we need to do everything before it
	const sview view = getView();
	return { view, std::move(std::move(*this).consumeSource()), optionBegin, optionEnd, paramDelimiter, optionDelimiter };
}

bool Option::empty() const {
	return getView().empty();
}

double Option::parseNumber(sview source) {
	MathExpressionParser parser(source);

	parser.parse();

	if (parser.isError()) {
		return 0;
	}

	auto exp = parser.getExpression();
	exp.solve();

	if (exp.type != ExpressionType::eNUMBER) {
		return 0;
	}

	return exp.number;
}

std::map<SubstringViewInfo, SubstringViewInfo> Option::parseMapParams(sview source, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	auto list = Tokenizer::parse(source, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };
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