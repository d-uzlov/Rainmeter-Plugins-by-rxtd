/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "pdh/PdhWrapper.h"
#include "pdh/NamesManager.h"
#include "BlacklistManager.h"
#include "enums.h"
#include "expressions.h"

namespace rxtd::perfmon {
	using counter_t = pdh::counter_t;
	using item_t = pdh::item_t;

	class ExpressionResolver;

	struct Indices {
		item_t current;
		item_t previous;
	};

	struct InstanceInfo {
		sview sortName;
		double sortValue;
		Indices indices;
		std::vector<Indices> vectorIndices;
	};

	class InstanceManager {
	public:
		enum class SortBy {
			eNONE,
			eINSTANCE_NAME,
			eRAW_COUNTER,
			eFORMATTED_COUNTER,
			eEXPRESSION,
			eROLLUP_EXPRESSION,
			eCOUNT,
		};

		enum class SortOrder {
			eASCENDING,
			eDESCENDING,
		};

	private:
		utils::Rainmeter::Logger& log;

		bool keepDiscarded = false;
		bool syncRawFormatted = true;
		bool rollup = false;
		index indexOffset = 0;
		bool limitIndexOffset = false;

		SortBy sortBy = SortBy::eNONE;
		index sortIndex = 0;
		SortOrder sortOrder = SortOrder::eDESCENDING;
		RollupFunction sortRollupFunction = RollupFunction::eSUM;

		const pdh::PdhWrapper& pdhWrapper;

		std::vector<InstanceInfo> instances;
		std::vector<InstanceInfo> instancesRolledUp;
		std::vector<InstanceInfo> instancesDiscarded;

		const pdh::PdhSnapshot& idSnapshot;
		const pdh::PdhSnapshot& snapshotCurrent;
		const pdh::PdhSnapshot& snapshotPrevious;
		const BlacklistManager& blacklistManager;

		pdh::NamesManager namesCurrent;
		pdh::NamesManager namesPrevious;

		// (use orig name, partial match, name) -> instanceInfo
		mutable std::map<std::tuple<bool, bool, sview>, std::optional<const InstanceInfo*>> nameCache;
		mutable decltype(nameCache) nameCacheRollup;
		mutable decltype(nameCache) nameCacheDiscarded;

	public:
		InstanceManager(
			utils::Rainmeter::Logger& log, const pdh::PdhWrapper& phWrapper, const pdh::PdhSnapshot& idSnapshot,
			const pdh::PdhSnapshot& snapshotCurrent, const pdh::PdhSnapshot& snapshotPrevious,
			const BlacklistManager& blacklistManager
		);

		void setKeepDiscarded(bool value);
		void setSyncRawFormatted(bool value);
		void setRollup(bool value);
		void setIndexOffset(item_t value);
		void setLimitIndexOffset(bool value);

		void setSortIndex(counter_t value);
		void setSortBy(SortBy value);
		void setSortOrder(SortOrder value);
		void setSortRollupFunction(RollupFunction value);

		item_t getIndexOffset() const;
		bool isRollup() const;
		counter_t getCountersCount() const;

		const pdh::ModifiedNameItem& getNames(index index) const;

		void checkIndices(counter_t counters, counter_t expressions, counter_t rollupExpressions);

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

		const InstanceInfo* findInstance(const Reference& ref, item_t sortedIndex) const;

		const InstanceInfo* findInstanceByName(const Reference& ref, bool useRollup) const;

		double calculateRaw(counter_t counterIndex, Indices originalIndexes) const;

		double calculateFormatted(counter_t counterIndex, Indices originalIndexes) const;


	private:
		void buildInstanceKeysZero();

		void buildInstanceKeys();

		void buildRollupKeys();

		item_t findPreviousName(sview uniqueName, item_t hint) const;

		const InstanceInfo* findInstanceByNameInList(
			const Reference& ref,
			const std::vector<InstanceInfo>& instances, std::map<std::tuple<bool, bool, sview>,
			                                                     std::optional<const InstanceInfo*>>& cache
		) const;

	};
}
