/* 
 * Copyright (C) 2019 rxtd
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
	public:
		enum class SortBy {
			NONE,
			INSTANCE_NAME,
			RAW_COUNTER,
			FORMATTED_COUNTER,
			EXPRESSION,
			ROLLUP_EXPRESSION,
			COUNT,
		};
		enum class SortOrder {
			ASCENDING,
			DESCENDING,
		};

	private:
		enum class TotalSource {
			RAW_COUNTER,
			FORMATTED_COUNTER,
			EXPRESSION,
			ROLLUP_EXPRESSION,
		};

		utils::Rainmeter::Logger &log;

		const InstanceManager &instanceManager;

		std::vector<ExpressionTreeNode> expressions;
		std::vector<ExpressionTreeNode> rollupExpressions;

		mutable const InstanceInfo* expressionCurrentItem = nullptr;

		// (source, counterIndex, rollupFunction) -> value
		mutable std::map<std::tuple<TotalSource, index, RollupFunction>, std::optional<double>> totalsCache;

	public:
		ExpressionResolver(utils::Rainmeter::Logger& log, const InstanceManager& instanceManager);

		unsigned getExpressionsCount() const;

		unsigned getRollupExpressionsCount() const;

		void resetCaches();

		double getValue(const Reference& ref, const InstanceInfo* instance, utils::Rainmeter::Logger& logger) const;

		void setExpressions(utils::OptionParser::OptionList expressionsList, utils::OptionParser::OptionList rollupExpressionsList);

		double getRaw(index counterIndex, Indices originalIndexes) const;

		double getFormatted(index counterIndex, Indices originalIndexes) const;

		double getRawRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		double getFormattedRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		double getExpressionRollup(RollupFunction rollupType, index expressionIndex, const InstanceInfo& instance) const;

		double getExpression(unsigned expressionIndex, const InstanceInfo& instance) const;

		double getRollupExpression(unsigned expressionIndex, const InstanceInfo& instance) const;

	private:
		double calculateTotal(TotalSource source, unsigned counterIndex, RollupFunction rollupFunction) const;

		const InstanceInfo* findAndCacheName(const Reference& ref, bool useRollup) const;

		double calculateAndCacheTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const;

		double resolveReference(const Reference& ref) const;

		double calculateExpressionRollup(const ExpressionTreeNode& expression, RollupFunction rollupFunction) const;

		double calculateCountTotal(RollupFunction rollupFunction) const;

		double calculateRollupCountTotal(RollupFunction rollupFunction) const;

		double resolveRollupReference(const Reference& ref) const;


		template <double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
		double calculateRollup(RollupFunction rollupType, index counterIndex, const InstanceInfo& instance) const;

		template <double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
		double calculateTotal(RollupFunction rollupType, index counterIndex) const;

		template <double(ExpressionResolver::* calculateExpressionFunction)(const ExpressionTreeNode& expression) const>
		double calculateExpressionTotal(RollupFunction rollupType, const ExpressionTreeNode& expression, bool rollup) const;

		template <double(ExpressionResolver::* resolveReferenceFunction)(const Reference& ref) const>
		double calculateExpression(const ExpressionTreeNode& expression) const;


		static bool indexIsInBounds(index ind, index min, index max);
	};
}
