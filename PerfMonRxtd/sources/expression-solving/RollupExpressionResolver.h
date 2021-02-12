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
	class RollupExpressionResolver {
		using Logger = ::rxtd::common::rainmeter::Logger;
		using OptionList = ::rxtd::common::options::OptionList;
		using ASTSolver = common::expressions::ASTSolver;

		class ReferenceResolver : public common::expressions::ASTSolver::ValueProvider {
			const RollupExpressionResolver& expressionResolver;
			const InstanceInfo& instanceInfo;

		public:
			ReferenceResolver(const RollupExpressionResolver& expressionResolver, const InstanceInfo& instanceInfo) :
				expressionResolver(expressionResolver), instanceInfo(instanceInfo) {}

			std::optional<double> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return expressionResolver.resolveReference(ref, instanceInfo);
			}
		};

		Logger log;

		const InstanceManager& instanceManager;
		ExpressionSolver& simpleExpressionSolver;

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

		double calculateExpressionRollup(index expressionIndex, RollupFunction rollupFunction, const InstanceInfo& instance) const;

		double resolveReference(const Reference& ref, const InstanceInfo& instanceInfo) const;

	private:
		double getRaw(index counterIndex, Indices originalIndexes) const;

		double getFormatted(index counterIndex, Indices originalIndexes) const;
		
		double calculateRollupCountTotal(RollupFunction rollupFunction) const;

		double solveExpression(index expressionIndex, const InstanceInfo& instanceInfo) const;
		
		template<typename Callable>
		double doRollup(
			RollupFunction rollupFunction,
			Indices initial,
			array_view<Indices> indices,
			Callable callable
		) const {
			double value = callable(initial);

			switch (rollupFunction) {
			case RollupFunction::eSUM: {
				for (auto item : indices) {
					value += callable(item);
				}
				break;
			}
			case RollupFunction::eAVERAGE: {
				for (auto item : indices) {
					value += callable(item);
				}
				value /= double(indices.size() + 1);
				break;
			}
			case RollupFunction::eMINIMUM: {
				for (auto item : indices) {
					value = std::min(value, callable(item));
				}
				break;
			}
			case RollupFunction::eMAXIMUM: {
				for (auto item : indices) {
					value = std::max(value, callable(item));
				}
				break;
			}
			case RollupFunction::eFIRST:
				break;
			}

			return value;
		}
	};
}
