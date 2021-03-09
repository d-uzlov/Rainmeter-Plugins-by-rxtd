// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

#include "Option.h"
#include "rxtd/Logger.h"
#include "rxtd/expression_parser/ASTParser.h"
#include "rxtd/std_fixes/MyMath.h"

namespace rxtd::option_parsing {
	class OptionParser {
		Logger logger;
		expression_parser::ASTParser parser;

	public:
		class Exception : public std::runtime_error {
		public:
			explicit Exception() : runtime_error("") {}
		};

		class ParseContext {
			OptionParser& parent;
			sview source;
			sview loggerPrefix;

		public:
			struct hex_tag {};

			ParseContext(OptionParser& parent, const sview& value, const sview& loggerPrefix) :
				parent(parent), source(value), loggerPrefix(loggerPrefix) {}

			/// On error writes a log message and throws OptionParser::Exception.
			/// Treats empty source value as error
			template<typename T>
			T as() {
				if (source.empty()) {
					parent.logger.error(L"{}: required value can't be empty", loggerPrefix);
					throw Exception{};
				}
				return solve<T>();
			}

			/// On error writes a log message and throws OptionParser::Exception.
			template<typename T>
			T valueOr(T defaultValue) {
				if (source.empty()) {
					return defaultValue;
				}
				return solve<T>();
			}

			template<typename T, typename ... Args>
			T asCustomOr(T defaultValue, const Args& ...args) {
				if (source.empty()) {
					return defaultValue;
				}
				return solveCustom<T>(args...);
			}

			template<typename T, typename ... Args>
			T asCustom(const Args& ...args) {
				if (source.empty()) {
					parent.logger.error(L"{}: required value can't be empty", loggerPrefix);
					throw Exception{};
				}
				return solveCustom<T>(args...);
			}

		private:
			template<typename T, typename ... Args>
			T solveCustom(const Args& ...args) {
				// ReSharper disable once CppStaticAssertFailure
				static_assert(false, "unknown custom type");
				return {};
			}

			template<typename T>
			T solveCustom(const hex_tag&) {
				// todo add proper error logging
				return static_cast<T>(std_fixes::StringUtils::parseInt(source, true));
			}

			template<typename T>
			T solve() {
				if constexpr (std::is_same<T, bool>::value) {
					if (source % ciView() == L"true") {
						return true;
					}
					if (source % ciView() == L"false") {
						return false;
					}
					return !std_fixes::MyMath::checkFloatEqual(parent.parseFloatImpl(source, loggerPrefix), 0.0);
				} else if constexpr (std::is_integral<T>::value) {
					const auto dVal = parent.parseFloatImpl(source, loggerPrefix);
					if (dVal > static_cast<double>(std::numeric_limits<T>::max()) ||
						dVal < static_cast<double>(std::numeric_limits<T>::lowest())) {
						parent.logger.error(L"{}: value is out of bounds", loggerPrefix);
						throw Exception{};
					}
					return std_fixes::MyMath::roundTo<T>(dVal);
				} else if constexpr (std::is_floating_point<T>::value) {
					return static_cast<T>(parent.parseFloatImpl(source, loggerPrefix));
				} else {
					// ReSharper disable once CppStaticAssertFailure
					static_assert(false, "unknown type");
					return {};
				}
			}
		};

		OptionParser() = default;

		static OptionParser getDefault();

		void setGrammar(const expression_parser::GrammarDescription& grammar) {
			parser.setGrammar(grammar, false);
		}

		void setLogger(Logger value) {
			logger = std::move(value);
		}

		[[nodiscard]]
		ParseContext parse(const OptionMap& map, sview optionName) {
			return ParseContext{ *this, map.get(optionName % ciView()).asString(), optionName };
		}

		[[nodiscard]]
		ParseContext parse(const Option& opt, sview loggerPrefix) {
			return ParseContext{ *this, opt.asString(), loggerPrefix };
		}

		[[nodiscard]]
		ParseContext parse(sview source, sview loggerPrefix) {
			return ParseContext{ *this, source, loggerPrefix };
		}

	private:
		/// <summary>
		/// Throws OptionParser::Exception
		/// </summary>
		/// <param name="source">Text to be parsed</param>
		/// <param name="loggerPrefix">Prefix that will be used for log message in form "<prefix>: <message>"</param>
		/// <returns>Parsed value</returns>
		double parseFloatImpl(sview source, sview loggerPrefix);
	};
}
