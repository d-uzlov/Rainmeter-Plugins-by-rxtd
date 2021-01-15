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
#include "InstanceManager.h"
#include "expressions.h"

namespace rxtd::perfmon {
	class ExpressionResolver {
	private:
		enum class TotalSource {
			eRAW_COUNTER,
			eFORMATTED_COUNTER,
			eEXPRESSION,
			eROLLUP_EXPRESSION,
		};

		utils::Rainmeter::Logger& log;

		const InstanceManager& instanceManager;

		std::vector<ExpressionTreeNode> expressions;
		std::vector<ExpressionTreeNode> rollupExpressions;

		mutable const InstanceInfo* expressionCurrentItem = nullptr;

		struct CacheEntry {
			TotalSource source;
			index counterIndex;
			RollupFunction rollupFunction;

			bool operator<(const CacheEntry& other) const;
		};

		mutable std::map<CacheEntry, std::optional<double>> totalsCache;

	public:
		ExpressionResolver(utils::Rainmeter::Logger& log, const InstanceManager& instanceManager);

		index getExpressionsCount() const;

		index getRollupExpressionsCount() const;

		void resetCaches();

		double getValue(const Reference& ref, const InstanceInfo* instance, utils::Rainmeter::Logger& logger) const;

		void setExpressions(utils::OptionList expressionsList, utils::OptionList rollupExpressionsList);

		double getRaw(index counterIndex, Indices originalIndexes) const;

		double getFormatted(index counterIndex, Indices originalIndexes) const;

		double getRawRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		double getFormattedRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		double getExpressionRollup(RollupFunction rollupType, index expressionIndex, const InstanceInfo& instance) const;

		double getExpression(index expressionIndex, const InstanceInfo& instance) const;

		double getRollupExpression(index expressionIndex, const InstanceInfo& instance) const;

	private:
		double calculateTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const;

		double calculateAndCacheTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const;

		double resolveReference(const Reference& ref) const;

		double calculateExpressionRollup(const ExpressionTreeNode& expression, RollupFunction rollupFunction) const;

		double calculateCountTotal(RollupFunction rollupFunction) const;

		double calculateRollupCountTotal(RollupFunction rollupFunction) const;

		double resolveRollupReference(const Reference& ref) const;


		template<double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
		double calculateRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		template<double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
		double calculateTotal(RollupFunction rollupType, index counterIndex) const;

		template<double(ExpressionResolver::* calculateExpressionFunction)(const ExpressionTreeNode& expression) const>
		double calculateExpressionTotal(RollupFunction rollupType, const ExpressionTreeNode& expression, bool rollup) const;

		template<double(ExpressionResolver::* resolveReferenceFunction)(const Reference& ref) const>
		double calculateExpression(const ExpressionTreeNode& expression) const;


		static bool indexIsInBounds(index ind, index min, index max);
	};
}
