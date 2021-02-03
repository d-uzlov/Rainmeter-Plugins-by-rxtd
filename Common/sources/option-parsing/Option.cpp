/*
 * Copyright (C) 2019-2021 rxtd
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
#include "rainmeter/Logger.h"

using namespace common::options;
using StringUtils = utils::StringUtils;

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

std::pair<Option, Option> Option::breakFirst(wchar_t separator) const & {
	sview view = getView();

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { *this, {} };
	}

	auto first = Option{ StringUtils::trim(sview(view.data(), delimiterPlace)) };
	auto rest = Option{ StringUtils::trim(sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1)) };
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

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) const & {
	return { getView(), optionBegin, optionEnd, optionDelimiter };
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) && {
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), optionBegin, optionEnd, optionDelimiter };
	} else {
		return { getView(), optionBegin, optionEnd, optionDelimiter };
	}
}

double Option::parseNumber(sview source) {
	utils::MathExpressionParser parser(source);

	parser.parse();

	if (parser.isError()) {
		common::buffer_printer::BufferPrinter printer;
		printer.print(L"can't parse '{}' as a number", source);
		common::rainmeter::Logger::sourcelessLog(printer.getBufferPtr());
		return 0;
	}

	auto exp = parser.getExpression();
	exp.solve();

	if (exp.type != utils::ExpressionType::eNUMBER) {
		return 0;
	}

	return exp.number;
}

std::wostream& rxtd::common::options::operator<<(std::wostream& stream, const Option& opt) {
	stream << opt.asString();
	return stream;
}
