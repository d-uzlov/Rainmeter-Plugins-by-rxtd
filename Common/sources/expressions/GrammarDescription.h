/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::common::expressions {
	class OperatorInfo {
	public:
		using PrecedenceType = int8_t;

		enum class Type : int8_t {
			ePREFIX,
			eBINARY,
			ePOSTFIX,
		};

		struct Data {
			double value = 0.0;

			Data() = default;

			explicit Data(double value) : value(value) {}

			void setTernaryFail() {
				value = std::numeric_limits<double>::quiet_NaN();
			}

			[[nodiscard]]
			bool isTernaryFail() const {
				return std::isnan(value);
			}
		};

		using OperatorFunction = Data(*)(Data, Data);

		class FunctionException : public std::runtime_error {
		public:
			explicit FunctionException() : runtime_error("") {}
		};

		class DataTypeMismatchException : public FunctionException {
		public:
			explicit DataTypeMismatchException() = default;
		};

	private:
		sview value;
		OperatorFunction function = nullptr;
		Type type = Type::eBINARY;
		PrecedenceType precedence = 0;
		PrecedenceType rightPrecedence = 0;
		PrecedenceType nextPrecedence = 0;

	public:
		OperatorInfo() = default;

		OperatorInfo(
			sview value,
			Type type,
			PrecedenceType precedence, PrecedenceType rightPrecedence, PrecedenceType nextPrecedence,
			OperatorFunction function
		):
			value(value),
			function(function),
			type(type),
			precedence(precedence),
			rightPrecedence(rightPrecedence),
			nextPrecedence(nextPrecedence) {}

		[[nodiscard]]
		auto getValue() const {
			return value;
		}

		[[nodiscard]]
		auto getType() const {
			return type;
		}

		[[nodiscard]]
		auto getMainPrecedence() const {
			return precedence;
		}

		[[nodiscard]]
		auto getRightPrecedence() const {
			return rightPrecedence;
		}

		[[nodiscard]]
		auto getNextPrecedence() const {
			return nextPrecedence;
		}

		[[nodiscard]]
		auto getFunction() const {
			return function;
		}
	};

	struct GroupingOperatorInfo {
		sview begin;
		sview end;
		sview separator;

		GroupingOperatorInfo() = default;

		GroupingOperatorInfo(const sview& begin, const sview& end, const sview& separator) :
			begin(begin), end(end), separator(separator) {}
	};

	struct GrammarDescription {
		/// <summary>
		/// Field operators contains unary prefix, unary postfix and binary operators
		/// For example: '+', '<=', '^'
		/// </summary>
		std::vector<OperatorInfo> operators{};

		/// <summary>
		/// Field groupingOperators contains paired operators, than don't have fixed arity
		/// For example: '()', '[]'s
		/// </summary>
		std::vector<GroupingOperatorInfo> groupingOperators{};
	};
}
