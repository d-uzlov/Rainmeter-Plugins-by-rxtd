/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <unordered_map>


#include "CacheHelper.h"
#include "ExpressionParser.h"
#include "ExpressionSolver.h"
#include "InstanceManager.h"
#include "Reference.h"
#include "expressions/ASTSolver.h"

namespace rxtd::perfmon {
	class RollupExpressionResolver {
		using Logger = ::rxtd::common::rainmeter::Logger;
		using OptionList = ::rxtd::common::options::OptionList;
		using ASTSolver = common::expressions::ASTSolver;

		class ReferenceResolver : public common::expressions::ASTSolver::ValueProvider {
			const RollupExpressionResolver& expressionResolver;
			array_view<Indices> indices;

		public:
			ReferenceResolver(const RollupExpressionResolver& expressionResolver, array_view<Indices> indices) :
				expressionResolver(expressionResolver), indices(indices) {}

			std::optional<double> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return expressionResolver.resolveReference(ref, indices);
			}
		};

		Logger log;

		const InstanceManager& instanceManager;
		ExpressionSolver& simpleExpressionSolver;

		ExpressionParser parser;

		std::vector<ASTSolver> expressions;

		struct CacheKey {
			index counterIndex;
			RollupFunction rollupFunction;

			bool operator<(const CacheKey& other) const {
				return counterIndex < other.counterIndex
					&& rollupFunction < other.rollupFunction;
			}
		};

		using Cache = CacheHelper<CacheKey, double>;

		struct TotalCaches {
			Cache raw;
			Cache formatted;
			Cache simpleExpression;
			Cache expression;

			void reset() {
				raw.reset();
				formatted.reset();
				simpleExpression.reset();
				expression.reset();
			}
		};

		mutable TotalCaches totalCaches;

	public:
		RollupExpressionResolver(Logger log, const InstanceManager& instanceManager, ExpressionSolver& simpleExpressionSolver);

		index getExpressionsCount() const;

		void resetCaches();

	private:
		ASTSolver parseExpressionTree(sview expressionString, sview loggerName, index loggerIndex);

		void parseExpressions(std::vector<ASTSolver>& vector, OptionList list, sview loggerName);

		void checkExpressionIndices();

	public:
		void setExpressions(OptionList expressionsList);

		double calculateExpressionRollup(index expressionIndex, RollupFunction rollupFunction, array_view<Indices> indices) const;

		double resolveReference(const Reference& ref, array_view<Indices> indices) const;

	private:
		double calculateRollupCountTotal(RollupFunction rollupFunction) const;

		double solveExpression(index expressionIndex, array_view<Indices> indices) const;
	};
}
