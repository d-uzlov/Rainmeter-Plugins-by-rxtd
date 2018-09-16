/* Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <string>
#include <vector>
#include "enums.h"

namespace pmrexp {
	enum expressionType {
		EXP_TYPE_UNKNOWN,
		EXP_TYPE_NUMBER,
		EXP_TYPE_SUM,
		EXP_TYPE_DIFF,
		EXP_TYPE_INVERSE,
		EXP_TYPE_MULT,
		EXP_TYPE_DIV,
		EXP_TYPE_POWER,
		EXP_TYPE_REF,
	};

	enum referenceType {
		REF_TYPE_UNKNOWN,
		REF_TYPE_COUNTER_RAW,
		REF_TYPE_COUNTER_FORMATTED,
		REF_TYPE_EXPRESSION,
		REF_TYPE_ROLLUP_EXPRESSION,
	};

	struct ExpressionTreeNode;

	struct reference {
		referenceType type = REF_TYPE_UNKNOWN;
		int counter = 0;
		int number = 0;
		std::wstring name = L"";
		std::wstring uniqueName = L"";
		bool named = false;
		bool namePartialMatch = false;
		bool useOrigName = false;
		pmre::rollupFunctionType rollupFunction = pmre::ROLLUP_SUM;
		pmre::nameSearchPlace searchPlace = pmre::NSP_PASSED;
	};

	struct ExpressionTreeNode {
		expressionType type = EXP_TYPE_UNKNOWN;
		std::vector<ExpressionTreeNode> nodes;
		reference ref;
		double number = 0.0;

		void simplify();
		int maxExpRef();
		int maxRUERef();
		void processRefs(void (*handler)(reference&));
	};

	enum lexemeType {
		LEXEME_TYPE_UNKNOWN,
		LEXEME_TYPE_END,
		LEXEME_TYPE_NUMBER,
		LEXEME_TYPE_DOT,
		LEXEME_TYPE_PLUS,
		LEXEME_TYPE_MINUS,
		LEXEME_TYPE_MULT,
		LEXEME_TYPE_DIV,
		LEXEME_TYPE_POWER,
		LEXEME_TYPE_PAR_OPEN,
		LEXEME_TYPE_PAR_CLOSE,
		LEXEME_TYPE_WORD,
		LEXEME_TYPE_BR_OPEN,
		LEXEME_TYPE_BR_CLOSE,
		LEXEME_TYPE_HASH,
	};

	struct Lexeme {
		lexemeType type = LEXEME_TYPE_UNKNOWN;
		std::wstring value;
	};

	class Lexer {
	private:
		std::wstring source;
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
	private:
		std::wstring source;
		bool error = false;
		Lexer lexer;
		Lexeme next;
		ExpressionTreeNode result;
		ExpressionTreeNode ret;

	public:
		Parser(const std::wstring& source);
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
