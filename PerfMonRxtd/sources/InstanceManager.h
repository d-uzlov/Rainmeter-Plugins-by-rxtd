/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <vector>
#include "pdh/PdhWrapper.h"
#include "pdh/NamesManager.h"
#include "BlacklistManager.h"
#include "enums.h"
#include "expressions.h"

#undef min
#undef max

namespace rxpm {
	class ExpressionResolver;

	struct Indices {
		unsigned long current;
		unsigned long previous;
	};

	struct InstanceInfo {
		std::wstring_view sortName;
		double sortValue;
		Indices indices;
		std::vector<Indices> vectorIndices;
	};

	class InstanceManager {
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
		static constexpr size_t NO_INDEX = static_cast<size_t>(-1);

		rxu::Rainmeter::Logger &log;

		bool keepDiscarded = false;
		bool syncRawFormatted = true;
		bool rollup = false;
		int indexOffset = 0;
		bool limitIndexOffset = false;

		SortBy sortBy = SortBy::NONE;
		unsigned sortIndex = 0;
		SortOrder sortOrder = SortOrder::DESCENDING;
		RollupFunction sortRollupFunction = RollupFunction::SUM;

		const pdh::PdhWrapper &pdhWrapper;

		std::vector<InstanceInfo> instances;
		std::vector<InstanceInfo> instancesRolledUp;
		std::vector<InstanceInfo> instancesDiscarded;

		const pdh::Snapshot &idSnapshot;
		const pdh::Snapshot &snapshotCurrent;
		const pdh::Snapshot &snapshotPrevious;
		const BlacklistManager &blacklistManager;

		pdh::NamesManager namesCurrent;
		pdh::NamesManager namesPrevious;

		// (use orig name, partial match, name) -> instanceInfo
		mutable std::map<std::tuple<bool, bool, std::wstring_view>, std::optional<const InstanceInfo*>> nameCache;
		mutable decltype(nameCache) nameCacheRollup;
		mutable decltype(nameCache) nameCacheDiscarded;

	public:
		InstanceManager(
			rxu::Rainmeter::Logger& log, const pdh::PdhWrapper& phWrapper, const pdh::Snapshot& idSnapshot,
			const pdh::Snapshot& snapshotCurrent, const pdh::Snapshot& snapshotPrevious,
			const BlacklistManager& blacklistManager
		);

		void setKeepDiscarded(bool value);
		void setSyncRawFormatted(bool value);
		void setRollup(bool value);
		void setIndexOffset(int value);
		void setLimitIndexOffset(bool value);

		void setSortIndex(int value);
		void setSortBy(SortBy value);
		void setSortOrder(SortOrder value);
		void setSortRollupFunction(RollupFunction value);

		int getIndexOffset() const;
		bool isRollup() const;
		unsigned getCountersCount() const;

		const pdh::ModifiedNameItem& getNames(size_t index) const;

		void checkIndices(unsigned counters, unsigned expressions, unsigned rollupExpressions);

		void update();

		void sort(const ExpressionResolver& expressionResolver);

		const std::vector<InstanceInfo>& getInstances() const;

		const std::vector<InstanceInfo>& getDiscarded() const;

		const std::vector<InstanceInfo>& getRollupInstances() const;

		/** We only need one snapshot for raw values, but if sync is enabled then we'll wait for two snapshots */
		bool canGetRaw() const;
		/** We need two complete snapshots for formatted values values */
		bool canGetFormatted() const;

		void setNameModificationType(pdh::NamesManager::ModificationType value);

		const InstanceInfo* findInstance(const Reference& ref, size_t sortedIndex) const;

		const InstanceInfo* findInstanceByName(const Reference& ref, bool useRollup) const;

		long long calculateRaw(unsigned counterIndex, const Indices& originalIndexes) const;

		double calculateFormatted(unsigned counterIndex, const Indices& originalIndexes) const;


	private:
		void buildInstanceKeysZero();

		void buildInstanceKeys();

		void buildRollupKeys();

		size_t findPreviousName(std::wstring_view uniqueName, size_t hint) const;

		const InstanceInfo* findInstanceByNameInList(
			const Reference& ref,
			const std::vector<InstanceInfo>& instances, std::map<std::tuple<bool, bool, std::wstring_view>,
			std::optional<const InstanceInfo*>>&cache) const;

	};
}
