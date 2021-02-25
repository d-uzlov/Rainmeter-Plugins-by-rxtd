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
	struct GrammarDescription {
		class NodeData {
			double value = 0.0;

		public:
			NodeData() = default;

			NodeData(double value) : value(value) { }

			[[nodiscard]]
			auto getValue() const {
				return value;
			}

			[[nodiscard]]
			bool isTernaryFail() const {
				return std::isnan(value);
			}

			static NodeData makeTernaryFail() {
				NodeData result;
				result.value = std::numeric_limits<double>::quiet_NaN();
				return result;
			}
		};

		struct MainOperatorInfo {
			using SolveFunction = NodeData(*)(NodeData, NodeData);

			sview operatorValue{};
			SolveFunction solveFunction{};

			MainOperatorInfo() = default;

			MainOperatorInfo(
				sview operatorValue,
				SolveFunction solveFunction
			) : operatorValue(operatorValue), solveFunction(solveFunction) {}
		};

		class OperatorInfo {
		public:
			using PrecedenceType = int8_t;

			enum class Type : int8_t {
				ePREFIX,
				eBINARY,
				ePOSTFIX,
			};

			class FunctionException : public std::runtime_error {
			public:
				explicit FunctionException() : runtime_error("") {}
			};

			class DataTypeMismatchException : public FunctionException {
			public:
				explicit DataTypeMismatchException() = default;
			};

		private:
			MainOperatorInfo mainInfo;
			Type type{};
			PrecedenceType precedence = 0;
			PrecedenceType rightPrecedence = 0;
			PrecedenceType nextPrecedence = 0;

		public:
			OperatorInfo() = default;

			OperatorInfo(
				MainOperatorInfo mainInfo,
				Type type,
				PrecedenceType precedence, PrecedenceType rightPrecedence, PrecedenceType nextPrecedence
			) :
				mainInfo(mainInfo),
				type(type),
				precedence(precedence), rightPrecedence(rightPrecedence), nextPrecedence(nextPrecedence) {}

			[[nodiscard]]
			auto getMainInfo() const {
				return mainInfo;
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
		};

		struct GroupingOperatorInfo {
			sview begin;
			sview end;
			sview separator;

			GroupingOperatorInfo() = default;

			GroupingOperatorInfo(const sview& begin, const sview& end, const sview& separator) :
				begin(begin), end(end), separator(separator) {}
		};


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
