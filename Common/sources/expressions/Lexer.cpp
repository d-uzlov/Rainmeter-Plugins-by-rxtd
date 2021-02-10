/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Lexer.h"

#include "StringUtils.h"

using namespace rxtd::common::expressions;

Lexer::Lexeme Lexer::readNext(array_view<sview> additionalSymbols) {
	skipWhile(std::iswspace);

	if (position >= sourceLength) {
		return Lexeme(Type::eEND, {});
	}

	if (std::iswdigit(source[position])) {
		// This class intentionally recognizes numbers only in the form of
		//		<digits>.<digits>
		// Scientific notation is not supported,
		// because this class is intended to be a part of a math parsing algorithm,
		// so users can write "1.5 * 10 ^ -3" instead of "1.5e-3"

		const auto startPos = position;

		skipWhile(std::iswdigit);

		if (position < sourceLength && L'.' == source[position]) {
			position++;
			skipWhile(std::iswdigit);
		}

		return Lexeme{ Type::eNUMBER, source.substr(startPos, position - startPos) };
	}

	if (auto opt = tryParseSymbols(additionalSymbols);
		opt.has_value()) {
		return opt.value();
	}

	if (auto opt = tryParseSymbols(standardSymbols);
		opt.has_value()) {
		return opt.value();
	}

	if (std::iswalpha(source[position])) {
		const auto startPos = position;

		skipWhile(std::iswalpha);

		return Lexeme{ Type::eWORD, source.substr(startPos, position - startPos) };
	}

	throw Exception{ position };
}

Lexer::Lexeme Lexer::readUntil(sview symbols) {
	const auto symPosition = source.substr(position).find_first_of(symbols);

	if (symPosition == sview::npos) {
		const auto result = source.substr(position);
		position = sourceLength;
		return Lexeme{ Type::eWORD, result };
	}

	const auto result = source.substr(position, symPosition);
	position += symPosition;
	return Lexeme{ Type::eWORD, result };
}

std::optional<Lexer::Lexeme> Lexer::tryParseSymbols(array_view<sview> symbols) {
	for (sview op : symbols) {
		const bool found = utils::StringUtils::checkStartsWith(source.substr(position), op);
		if (found) {
			position += index(op.length());
			return Lexeme{ Type::eSYMBOL, op };
		}
	}

	return {};
}
