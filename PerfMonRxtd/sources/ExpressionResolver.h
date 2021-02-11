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

#include "ExpressionParser.h"
#include "InstanceManager.h"
#include "Reference.h"
#include "expressions/ASTSolver.h"

namespace rxtd::perfmon {
	class ExpressionResolver {
		using Logger = ::rxtd::common::rainmeter::Logger;
		using OptionList = ::rxtd::common::options::OptionList;
		using ASTSolver = common::expressions::ASTSolver;

		class ReferenceResolver : public common::expressions::ASTSolver::ValueProvider {
			const ExpressionResolver& expressionResolver;
			const InstanceInfo* instanceInfo = nullptr;
			Logger logger;

		public:
			ReferenceResolver(const ExpressionResolver& expressionResolver, const InstanceInfo* instanceInfo, Logger logger) :
				expressionResolver(expressionResolver), instanceInfo(instanceInfo), logger(std::move(logger)) {}

			std::optional<double> solveCustom(const std::any& value) override {
				auto& ref = *std::any_cast<Reference>(&value);
				return expressionResolver.getValue(ref, instanceInfo, logger);
			}
		};

		enum class TotalSource {
			eRAW_COUNTER,
			eFORMATTED_COUNTER,
			eEXPRESSION,
			eROLLUP_EXPRESSION,
		};

		Logger log;

		const InstanceManager& instanceManager;

		ExpressionParser parser;

		std::vector<ASTSolver> expressions;
		std::vector<ASTSolver> rollupExpressions;

		mutable const InstanceInfo* expressionCurrentItem = nullptr;


		struct CacheEntry {
			TotalSource source;
			index counterIndex;
			RollupFunction rollupFunction;

			bool operator<(const CacheEntry& other) const;
		};

		mutable std::map<CacheEntry, std::optional<double>> totalsCache;

	public:
		ExpressionResolver(Logger log, const InstanceManager& instanceManager);

		index getExpressionsCount() const;

		index getRollupExpressionsCount() const;

		void resetCaches();

	private:
		ASTSolver parseExpressionTree(sview expressionString, sview loggerName, index loggerIndex);

		void parseExpressions(std::vector<ASTSolver>& vector, OptionList list, sview loggerName);

		void checkExpressionIndices();

	public:
		void setExpressions(OptionList expressionsList, OptionList rollupExpressionsList);

		double getValue(const Reference& ref, const InstanceInfo* instancePtr, Logger& logger) const;

	private:
		double getRaw(index counterIndex, Indices originalIndexes) const;

		double getFormatted(index counterIndex, Indices originalIndexes) const;

		double calculateTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const;

		double calculateAndCacheTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const;

		double resolveReference(const Reference& ref) const;

		double calculateExpressionRollup(const ExpressionTreeNode& expression, RollupFunction rollupFunction) const;

		double calculateCountTotal(RollupFunction rollupFunction) const;

		double calculateRollupCountTotal(RollupFunction rollupFunction) const;

		double resolveRollupReference(const Reference& ref) const;

		template<double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
		double calculateTotal(RollupFunction rollupType, index counterIndex) const;

		template<double(ExpressionResolver::* calculateExpressionFunction)(const ExpressionTreeNode& expression) const>
		double calculateExpressionTotal(RollupFunction rollupType, const ExpressionTreeNode& expression, bool rollup) const;

		double ExpressionResolver::solveExpression(index expressionIndex) const;


		static bool indexIsInBounds(index ind, index min, index max);

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

		template<typename Callable>
		double doTotal(
			RollupFunction rollupFunction,
			array_view<InstanceInfo> instances,
			Callable callable
		) const {
			double value = 0.0;

			switch (rollupFunction) {
			case RollupFunction::eSUM: {
				for (auto item : instances) {
					value += callable(item);
				}
				break;
			}
			case RollupFunction::eAVERAGE: {
				if (instances.empty()) {
					break;
				}
				for (auto item : instances) {
					value += callable(item);
				}
				value /= double(instances.size());
				break;
			}
			case RollupFunction::eMINIMUM: {
				value = std::numeric_limits<double>::max();
				for (auto item : instances) {
					value = std::min(value, callable(item));
				}
				break;
			}
			case RollupFunction::eMAXIMUM: {
				value = std::numeric_limits<double>::min();
				for (auto item : instances) {
					value = std::max(value, callable(item));
				}
				break;
			}
			case RollupFunction::eFIRST:
				if (!instances.empty()) {
					value = callable(instances[0]);
				}
				break;
			}

			return value;
		}
	};
}
