/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::expression_parser {
	/// <summary>
	/// Class Lexer separates string_view source into tokens.
	/// Possible token types can be seen in Lexer::Type:
	///		eEND
	///			End of source string.
	///		eNUMBER
	///			Number in the form "<digits>" or "<digits>.<digits>".
	///			Scientific notation is intentionally not recognized.
	///		eSYMBOL
	///			Custom symbol.
	///		eWORD
	///			Any sequence of letters.
	///
	/// Parsing precedence is as follows:
	/// end > number > symbol > word
	///
	/// Symbols can consist of any character sequences,
	///	so you can add symbol "and", and Lexer will recognize it as a symbol instead of word.
	///
	///	However, since numbers have greater precedence,
	/// you can't parse any certain numbers as symbols.
	///
	/// For the same reason you can't have symbols that start with digits:
	/// when such symbol is encountered in the string, number will pe parsed first,
	/// and the symbol won't be recognized.
	/// </summary>
	class Lexer {
	public:
		enum class Type {
			eEND,
			eNUMBER,
			eSYMBOL,
			eWORD,
		};

		struct Lexeme {
			Type type{ Type::eEND };
			sview value;

			Lexeme() = default;

			Lexeme(Type type, sview value) :
				type(type),
				value(value) { }
		};

		class Exception : public std::runtime_error {
			index position;
		public:
			explicit Exception(index position) : runtime_error(""), position(position) {}

			[[nodiscard]]
			auto getPosition() const {
				return position;
			}
		};

	private:
		sview source;
		index position = 0;
		array_view<sview> standardSymbols;

		// its too annoying to deal with signed/unsigned mismatch
		// std::wstring_view#length() is unsigned but everything else is signed
		index sourceLength = 0;

	public:
		Lexer() = default;

		void setStandardSymbols(array_view<sview> value) {
			standardSymbols = value;
		}

		void setSource(sview value) {
			source = value;
			sourceLength = index(source.length());
			position = 0;
		}

		/// <returns>Returns current position</returns>
		[[nodiscard]]
		index getPosition() const {
			return position;
		}

		/// <summary>
		/// Reads next token and moves position to past the end of the token.
		/// Throws Lexer::Exception when encounters unknown symbol.
		/// Arg additionalSymbols can be used to temporarily add recognizable symbols
		/// without altering main list of recognized symbols.
		///
		/// additionalSymbols have greater precedence that standard symbols.
		/// </summary>
		///
		/// <returns>Lexeme that contain info about new token.</returns>
		Lexeme readNext(array_view<sview> additionalSymbols = {});

		Lexeme readUntil(sview symbols);

	private:
		std::optional<Lexeme> tryParseSymbols(array_view<sview> symbols);

		template<typename Predicate>
		void skipWhile(Predicate predicate) {
			static_assert(std::is_invocable_r<bool, Predicate, wchar_t>::value);

			while (position < sourceLength) {
				if (!predicate(source[position])) {
					break;
				}
				position++;
			}
		}
	};
}
