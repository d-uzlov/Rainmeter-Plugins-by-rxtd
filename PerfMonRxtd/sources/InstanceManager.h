/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "BlacklistManager.h"
#include "enums.h"
#include "expressions.h"
#include "pdh/NamesManager.h"
#include "pdh/PdhWrapper.h"

namespace rxtd::perfmon {
	class ExpressionResolver;

	struct Indices {
		int_fast16_t current;
		int_fast16_t previous;
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
		void setIndexOffset(index value);
		void setLimitIndexOffset(bool value);

		void setSortIndex(index value);
		void setSortBy(SortBy value);
		void setSortOrder(SortOrder value);
		void setSortRollupFunction(RollupFunction value);

		index getIndexOffset() const;
		bool isRollup() const;
		index getCountersCount() const;

		const pdh::ModifiedNameItem& getNames(index index) const;

		void checkIndices(index counters, index expressions, index rollupExpressions);

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

		const InstanceInfo* findInstance(const Reference& ref, index sortedIndex) const;

		const InstanceInfo* findInstanceByName(const Reference& ref, bool useRollup) const;

		double calculateRaw(index counterIndex, Indices originalIndexes) const;

		double calculateFormatted(index counterIndex, Indices originalIndexes) const;


	private:
		void buildInstanceKeysZero();

		void buildInstanceKeys();

		void buildRollupKeys();

		index findPreviousName(sview uniqueName, index hint) const;

		const InstanceInfo* findInstanceByNameInList(
			const Reference& ref,
			const std::vector<InstanceInfo>& instances, std::map<std::tuple<bool, bool, sview>,
			                                                     std::optional<const InstanceInfo*>>& cache
		) const;

	};
}
