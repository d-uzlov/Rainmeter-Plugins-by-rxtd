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
#include "InstanceManager.h"
#include "Reference.h"
#include "expressions/ASTSolver.h"

namespace rxtd::perfmon {
	class ExpressionSolver {
		using Logger = ::rxtd::common::rainmeter::Logger;
		using OptionList = ::rxtd::common::options::OptionList;
		using ASTSolver = common::expressions::ASTSolver;

		class ReferenceResolver : public common::expressions::ASTSolver::ValueProvider {
			const ExpressionSolver& expressionResolver;
			Indices indices;

		public:
			ReferenceResolver(const ExpressionSolver& expressionResolver, Indices indices) :
				expressionResolver(expressionResolver), indices(indices) {}

			std::optional<double> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return expressionResolver.resolveReference(ref, indices);
			}
		};

		Logger log;

		const InstanceManager& instanceManager;

		ExpressionParser parser;

		std::vector<ASTSolver> expressions;

		struct CacheEntry {
			index counterIndex;
			RollupFunction rollupFunction;

			bool operator<(const CacheEntry& other) const {
				return counterIndex < other.counterIndex
					&& rollupFunction < other.rollupFunction;
			}
		};

		using Cache = CacheHelper<CacheEntry, double>;

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
		ExpressionSolver(Logger log, const InstanceManager& instanceManager);

		index getExpressionsCount() const;

		void resetCache();

	private:
		ASTSolver parseExpression(sview expressionString, sview loggerName, index loggerIndex);

		void checkExpressionIndices();

	public:
		void setExpressions(OptionList expressionsList);

		double resolveReference(const Reference& ref, Indices indices) const;

		double solveExpression(index expressionIndex, Indices indices) {
			Reference ref;
			ref.type = Reference::Type::EXPRESSION;
			ref.counter = expressionIndex;
			return resolveReference(ref, indices);
		}

	private:
		double getRaw(index counterIndex, Indices originalIndexes) const {
			return instanceManager.calculateRaw(counterIndex, originalIndexes);
		}

		double getFormatted(index counterIndex, Indices originalIndexes) const {
			return instanceManager.calculateFormatted(counterIndex, originalIndexes);
		}

		double solveExpression(index expressionIndex, Indices indices) const;
	};
}
