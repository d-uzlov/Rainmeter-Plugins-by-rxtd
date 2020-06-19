/* 
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "enums.h"

namespace rxtd::perfmon {
	using counter_t = int16_t; // TODO unite using declarations
	using item_t = int16_t;

	enum class ExpressionType {
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

	enum class  ReferenceType : uint8_t {
		UNKNOWN,
		COUNTER_RAW,
		COUNTER_FORMATTED,
		EXPRESSION,
		ROLLUP_EXPRESSION,
		COUNT,
	};

	struct Reference {
		string name;
		counter_t counter = 0;
		RollupFunction rollupFunction = RollupFunction::eSUM;
		ReferenceType type = ReferenceType::UNKNOWN;
		bool discarded = false;
		bool named = false;
		bool namePartialMatch = false;
		bool useOrigName = false;
		bool total = false;
	};

	struct ExpressionTreeNode {
		Reference ref;
		std::vector<ExpressionTreeNode> nodes;
		double number = 0.0;
		ExpressionType type = ExpressionType::UNKNOWN;

		void simplify();
		counter_t maxExpRef() const;
		counter_t maxRUERef() const;
		void processRefs(void(*handler)(Reference&));
	};

	class ExpressionParser {
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
				sview value;

				Lexeme() = default;
				Lexeme(LexemeType type, sview value) :
					type(type),
					value(value) { }
			};

		private:
			sview source;
			index position = 0;

		public:
			explicit Lexer(sview source);
			Lexeme next();
			sview getUntil(wchar_t stop1, wchar_t stop2);

		private:
			void skipSpaces();
			static bool isSymbol(wchar_t c);
			sview readWord();
			sview readNumber();
		};

		string source { };
		Lexer lexer;
		Lexer::Lexeme next = { };
		ExpressionTreeNode result;
		bool error = false;

	public:
		explicit ExpressionParser(string source);
		explicit ExpressionParser(sview source);

		void parse();
		bool isError() const;
		ExpressionTreeNode getExpression() const;

	private:
		void readNext();
		ExpressionTreeNode parseExpression();
		ExpressionTreeNode parseTerm();
		ExpressionTreeNode parseFactor();
		ExpressionTreeNode parsePower();
		ExpressionTreeNode parseAtom();
		Reference parseReference();
	};
}
