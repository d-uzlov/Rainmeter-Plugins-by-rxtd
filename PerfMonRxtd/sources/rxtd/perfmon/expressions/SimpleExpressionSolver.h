// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once
#include <unordered_map>


#include "CacheHelper.h"
#include "rxtd/expression_parser/ASTSolver.h"
#include "rxtd/perfmon/ExpressionParser.h"
#include "rxtd/perfmon/Reference.h"
#include "rxtd/perfmon/instances/SimpleInstanceManager.h"

namespace rxtd::perfmon::expressions {
	class SimpleExpressionSolver {
		using OptionList = option_parsing::OptionList;
		using ASTSolver = expression_parser::ASTSolver;
		using Indices = SimpleInstanceManager::Indices;
		using InstanceInfo = SimpleInstanceManager::InstanceInfo;

		class ReferenceResolver : public expression_parser::ASTSolver::ValueProvider {
			const SimpleExpressionSolver& expressionResolver;
			Indices indices;

		public:
			ReferenceResolver(const SimpleExpressionSolver& expressionResolver, Indices indices) :
				expressionResolver(expressionResolver), indices(indices) {}

			std::optional<NodeData> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return NodeData{ expressionResolver.resolveReference(ref, indices) };
			}
		};

		Logger log;

		const SimpleInstanceManager& instanceManager;

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
			Cache expression;

			void reset() {
				raw.reset();
				formatted.reset();
				expression.reset();
			}
		};

		mutable TotalCaches totalCaches;

	public:
		SimpleExpressionSolver(Logger log, const SimpleInstanceManager& instanceManager);

		index getExpressionsCount() const;

		void resetCache();

	private:
		ASTSolver parseExpression(sview expressionString, sview loggerName, index loggerIndex);

		void checkExpressionIndices();

	public:
		void setExpressions(OptionList expressionsList);

		double resolveReference(const Reference& ref, Indices indices) const;

		double solveExpression(index expressionIndex, Indices indices) const;
	};
}
