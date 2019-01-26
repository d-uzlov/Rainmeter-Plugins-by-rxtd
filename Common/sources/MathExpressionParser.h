/* 
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace rxu {
	enum class ExpressionType {
		UNKNOWN,
		NUMBER,
		SUM,
		DIFF,
		INVERSE,
		MULT,
		DIV,
		POWER,
		TERNARY,
	};

	struct ExpressionTreeNode {
		std::vector<ExpressionTreeNode> nodes;
		double number = 0.0;
		ExpressionType type = ExpressionType::UNKNOWN;

		void solve();
	};

	class MathExpressionParser {
		class Lexer {
		public:
			enum class LexemeType {
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
				LexemeType type { LexemeType::UNKNOWN };
				std::wstring_view value;

				Lexeme() = default;
				Lexeme(LexemeType type, std::wstring_view value) :
					type(type),
					value(value) { }
			};

		private:
			std::wstring_view source;
			size_t position = 0;

		public:
			explicit Lexer(std::wstring_view source);
			Lexeme next();
			std::wstring_view getUntil(wchar_t stop1, wchar_t stop2);

		private:
			void skipSpaces();
			static bool isSymbol(wchar_t c);
			std::wstring_view readNumber();
		};

		std::wstring source { };
		Lexer lexer;
		Lexer::Lexeme next = { };
		ExpressionTreeNode result;
		bool error = false;

	public:
		explicit MathExpressionParser(std::wstring source);
		explicit MathExpressionParser(std::wstring_view source);

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

		static int64_t parseInt(std::wstring_view string);
		static double parseFractional(std::wstring_view string);
	};
}
