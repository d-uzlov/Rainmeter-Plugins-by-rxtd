/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "Lexer.h"
#include "SyntaxTree.h"
#include "GrammarDescription.h"

namespace rxtd::common::expressions {
	/// <summary>
	/// Class ASTParser can parse expressions in a given grammar.
	/// Usage:
	/// 1. Create grammar (see class expressions::GrammarBuilder) and call #setGrammar()
	/// 2. Parse AST from expression string by calling #parse().
	///
	/// Parsing algorithm is inspired by guide from Theodore Norvell
	/// http://www.engr.mun.ca/~theo/Misc/exp_parsing.htm
	/// However, algorithm is heavily modified to extend the range of supported grammars.
	/// </summary>
	class ASTParser {
	public:
		using OperatorInfo = GrammarDescription::OperatorInfo;
		using GroupingOperatorInfo = GrammarDescription::GroupingOperatorInfo;
		using IndexType = ast_nodes::IndexType;

		class Exception : public std::runtime_error {
			sview reason;
			index position;
		public:
			explicit Exception(sview reason, index position) : runtime_error(""), reason(reason), position(position) {}

			[[nodiscard]]
			auto getReason() const {
				return reason;
			}

			[[nodiscard]]
			auto getPosition() const {
				return position;
			}
		};

	private:
		/// <summary>
		/// One array of OperatorInfo in current grammar is split in two arrays
		/// for easier parsing of prefix and binary operators with the same name.
		/// For example, in classic syntax + and - may be unary or binary.
		/// Unary and binary versions have different precedence, and are parsed in different places,
		/// so finding the right operator for current case is very important.
		/// In different places different versions of such operators are expected,
		/// so it's convenient to split them into two separate fields.
		/// </summary>
		std::vector<OperatorInfo> opsInfoPrefix;
		/// <summary>
		/// See #opsInfoPrefix
		/// </summary>
		std::vector<OperatorInfo> opsInfoNonPrefix;

		Lexer lexer;
		/// <summary>
		/// Array for Lexer. Lexer doesn't manage memory for it's standard recognized symbols, so it's contained here.
		/// </summary>
		std::vector<sview> operatorSymbols;
		std::vector<GroupingOperatorInfo> groupingOperators;

		bool functionParsingIsAllowed = false;

	protected:
		Lexer::Lexeme next;
		SyntaxTree tree;

	public:
		ASTParser() = default;

		ASTParser(const ASTParser& other) = default;
		ASTParser(ASTParser&& other) noexcept = default;
		ASTParser& operator=(const ASTParser& other) = default;
		ASTParser& operator=(ASTParser&& other) noexcept = default;

		virtual ~ASTParser() = default;

		/// <summary>
		/// Changes grammar that this instance of ASTParser can parse.
		/// Higher precedence number correspond to higher precedence.
		/// 
		/// See expressions::GrammarBuilder class for easy creation of opsInfo.
		/// </summary>
		void setGrammar(const GrammarDescription& grammarDescription, bool allowFreeFunctions);

		/// <summary>
		/// Parses source as an expression in current grammar.
		/// 
		/// Will throw expressions::Lexer::Exception and ASTParser::Exception when source is invalid expression.
		/// </summary>
		void parse(sview source);

		[[nodiscard]]
		const SyntaxTree& getTree() const {
			return tree;
		}

		[[nodiscard]]
		SyntaxTree takeTree() {
			return std::exchange(tree, {});
		}

	protected:
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		virtual std::optional<ast_nodes::IndexType> parseCustom() {
			return {};
		}

		/// <summary>
		/// Parses a sequence of postfix or binary operators.
		/// 
		/// Throws ASTParser::Exception on any error
		/// </summary>
		ast_nodes::IndexType parseBinaryOrPostfix(index minAllowedPrecedence);

		/// <summary>
		/// Parses terminals, prefix values and custom values.
		/// 
		/// Throws ASTParser::Exception on any error
		/// </summary>
		ast_nodes::IndexType parsePrefixOrTerminalOrGroup();

		/// <summary>
		/// Tries to find matching operator in current grammar.
		/// @code prefixFirst determines whether prefix or postfix operators have precedence in search.
		/// 
		/// Throws ASTParser::Exception when operator corresponding to @code value is not found.
		/// </summary>
		[[nodiscard]]
		OperatorInfo parseOperator(sview value, bool prefixFirst = false) const;

		[[nodiscard]]
		std::optional<GroupingOperatorInfo> tryParseGroupingOperator(sview value) const;

		/// <summary>
		/// Reads next token from lexer.
		///
		/// See Lexer descriptions for information about @code additionalSymbols
		/// </summary>
		void skipToken(array_view<sview> additionalSymbols = {}) {
			next = lexer.readNext(additionalSymbols);
		}

		void readUntil(sview symbols) {
			next = lexer.readUntil(symbols);
		}

		[[noreturn]]
		void throwException(sview message) const {
			throw Exception{ message, index(lexer.getPosition() - next.value.length()) };
		}
	};
}
