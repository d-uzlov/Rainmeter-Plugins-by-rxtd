/* Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <string>
#include <utility>
#include <vector>
#include "enums.h"

namespace pmrexp {
	enum class ExpressionType : unsigned char {
		UNKNOWN,
		NUMBER,
		SUM,
		DIFF,
		INVERSE,
		MULT,
		DIV,
		POWER,
		REF,
		TERNARY,
	};

	enum class  ReferenceType : unsigned char {
		UNKNOWN,
		COUNTER_RAW,
		COUNTER_FORMATTED,
		EXPRESSION,
		ROLLUP_EXPRESSION,
		COUNT,
	};

	struct reference {
		ReferenceType type = ReferenceType::UNKNOWN;
		std::wstring name;
		int counter = 0;
		pmre::RollupFunction rollupFunction = pmre::RollupFunction::SUM;
		bool discarded = false;
		bool named = false;
		bool namePartialMatch = false;
		bool useOrigName = false;
		bool total = false;
	};

	struct ExpressionTreeNode {
		ExpressionType type = ExpressionType::UNKNOWN;
		std::vector<ExpressionTreeNode> nodes;
		double number = 0.0;
		reference ref;

		void simplify();
		int maxExpRef() const;
		int maxRUERef() const;
		void processRefs(void(*handler)(reference&));
	};

	enum class LexemeType : unsigned char {
		UNKNOWN,
		END,
		NUMBER,
		DOT,
		PLUS,
		MINUS,
		MULT,
		DIV,
		POWER,
		PAR_OPEN,
		PAR_CLOSE,
		WORD,
		BR_OPEN,
		BR_CLOSE,
		HASH,
		QMARK,
		COLON,
	};

	struct Lexeme {
		LexemeType type;
		std::wstring value;

		Lexeme(LexemeType type, std::wstring value)
			: type(type),
			value(std::move(value)) { }
	};

	class Lexer {
		const std::wstring source;
		int position = 0;

	public:
		Lexer(std::wstring source);
		Lexeme next();
		std::wstring getUntil(wchar_t stop1, wchar_t stop2);

	private:
		void skipSpaces();
		static bool isSymbol(wchar_t c);
		std::wstring readWord();
		std::wstring readNumber();
	};

	class Parser {
		bool error = false;
		Lexer lexer;
		Lexeme next = Lexeme(LexemeType::UNKNOWN, L"");
		ExpressionTreeNode result;

	public:
		Parser(std::wstring source);
		void parse();
		bool isError() const;
		ExpressionTreeNode getExpression() const;

	private:
		void readNext();
		static void toUpper(std::wstring& s);
		ExpressionTreeNode parseExpression();
		ExpressionTreeNode parseTerm();
		ExpressionTreeNode parseFactor();
		ExpressionTreeNode parsePower();
		ExpressionTreeNode parseAtom();
		reference parseReference();
	};
}
