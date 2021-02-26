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
#include "SimpleExpressionSolver.h"
#include "rxtd/expression_parser/ASTSolver.h"
#include "rxtd/perfmon/ExpressionParser.h"
#include "rxtd/perfmon/Reference.h"
#include "rxtd/perfmon/instances/RollupInstanceManager.h"
#include "rxtd/perfmon/instances/SimpleInstanceManager.h"

namespace rxtd::perfmon::expressions {
	class RollupExpressionResolver {
		using OptionList = option_parsing::OptionList;
		using ASTSolver = expression_parser::ASTSolver;
		using Indices = SimpleInstanceManager::Indices;
		using RollupInstanceInfo = RollupInstanceManager::RollupInstanceInfo;

		class ReferenceResolver : public ASTSolver::ValueProvider {
			const RollupExpressionResolver& expressionResolver;
			array_view<Indices> indices;

		public:
			ReferenceResolver(const RollupExpressionResolver& expressionResolver, array_view<Indices> indices) :
				expressionResolver(expressionResolver), indices(indices) {}

			std::optional<NodeData> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return NodeData{ expressionResolver.resolveReference(ref, indices) };
			}
		};

		Logger log;

		const SimpleInstanceManager& simpleInstanceManager;
		const RollupInstanceManager& rollupInstanceManager;
		SimpleExpressionSolver& simpleExpressionSolver;

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
		RollupExpressionResolver(
			Logger log,
			const SimpleInstanceManager& simpleInstanceManager, const RollupInstanceManager& rollupInstanceManager,
			SimpleExpressionSolver& simpleExpressionSolver
		) : log(std::move(log)),
		    simpleInstanceManager(simpleInstanceManager),
		    rollupInstanceManager(rollupInstanceManager),
		    simpleExpressionSolver(simpleExpressionSolver) {}

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
