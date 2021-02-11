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
#include "Reference.h"
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
		double sortValue = 0.0;
		Indices indices;
		std::vector<Indices> vectorIndices;
	};

	class InstanceManager {
	public:
		using Logger = ::rxtd::common::rainmeter::Logger;
		
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

		struct Options {
			bool keepDiscarded = false;
			bool syncRawFormatted = true;
			bool rollup = false;
			bool limitIndexOffset = false;

			SortBy sortBy = SortBy::eNONE;
			index sortIndex = 0;
			SortOrder sortOrder = SortOrder::eDESCENDING;
			RollupFunction sortRollupFunction = RollupFunction::eSUM;

			string blacklist;
			string blacklistOrig;
			string whitelist;
			string whitelistOrig;
		};

	private:
		Logger log;

		const pdh::PdhWrapper& pdhWrapper;

		Options options;
		index indexOffset = 0;

		std::vector<InstanceInfo> instances;
		std::vector<InstanceInfo> instancesRolledUp;
		std::vector<InstanceInfo> instancesDiscarded;

		pdh::PdhSnapshot snapshotCurrent;
		pdh::PdhSnapshot snapshotPrevious;
		pdh::PdhSnapshot processIdsSnapshot;
		BlacklistManager blacklistManager;

		std::vector<pdh::UniqueInstanceId> idsPrevious;
		std::vector<pdh::UniqueInstanceId> idsCurrent;
		pdh::NamesManager namesManager;

		// (use orig name, partial match, name) -> instanceInfo
		using CacheType = std::map<std::tuple<bool, bool, sview>, std::optional<const InstanceInfo*>>;
		mutable CacheType nameCache;
		mutable CacheType nameCacheRollup;
		mutable CacheType nameCacheDiscarded;

	public:
		InstanceManager(Logger log, const pdh::PdhWrapper& phWrapper);

		void setOptions(Options value) {
			options = value;

			if (options.limitIndexOffset && indexOffset < 0) {
				indexOffset = 0;
			}

			// TODO add check for maximum?
			if (options.sortIndex < 0) {
				log.error(L"SortIndex must be >= 0, but {} found, set to 0", value.sortIndex);
				options.sortIndex = 0;
			}

			blacklistManager.setLists(value.blacklist, value.blacklistOrig, value.whitelist, value.whitelistOrig);
		}

		void setIndexOffset(index value, bool relative) {
			if (relative) {
				indexOffset += value;
			} else {
				indexOffset = value;
			}
			if (options.limitIndexOffset && indexOffset < 0) {
				indexOffset = 0;
			}
		}

		bool isRollup() const {
			return options.rollup;
		}

		index getCountersCount() const {
			return snapshotCurrent.getCountersCount();
		}

		const pdh::ModifiedNameItem& getNames(index ind) const {
			return namesManager.get(ind);
		}

		const pdh::UniqueInstanceId& getIds(index ind) const {
			return idsCurrent[ind];
		}

		void checkIndices(index counters, index expressions, index rollupExpressions);

		void update();

		void sort(const ExpressionResolver& expressionResolver);

		array_view<InstanceInfo> getInstances() const {
			return instances;
		}

		array_view<InstanceInfo> getDiscarded() const {
			return instancesDiscarded;
		}

		array_view<InstanceInfo> getRollupInstances() const {
			return instancesRolledUp;
		}

		/** We only need one snapshot for raw values, but if sync is enabled then we'll wait for two snapshots */
		bool canGetRaw() const {
			return !snapshotCurrent.isEmpty() && (!options.syncRawFormatted || !snapshotPrevious.isEmpty());
		}

		/** We need two complete snapshots for formatted values values */
		bool canGetFormatted() const {
			return !snapshotCurrent.isEmpty() && !snapshotPrevious.isEmpty();
		}

		void setNameModificationType(pdh::NamesManager::ModificationType value);

		const InstanceInfo* findInstance(const Reference& ref, index sortedIndex) const;

		const InstanceInfo* findInstanceByName(const Reference& ref, bool useRollup) const;

		double calculateRaw(index counterIndex, Indices originalIndexes) const;

		double calculateFormatted(index counterIndex, Indices originalIndexes) const;

		[[nodiscard]]
		index getItemsCount() const {
			return snapshotCurrent.getItemsCount();
		}

		void swapSnapshot(pdh::PdhSnapshot& snapshot, pdh::PdhSnapshot& idsSnapshot) {
			std::swap(snapshotCurrent, snapshotPrevious);
			std::swap(snapshotCurrent, snapshot);
			std::swap(processIdsSnapshot, idsSnapshot);
		}

		void clear() {
			snapshotCurrent.clear();
			snapshotPrevious.clear();
		}

	private:
		void buildInstanceKeysZero();

		void buildInstanceKeys();

		void buildRollupKeys();

		index findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const;

		const InstanceInfo* findInstanceByNameInList(const Reference& ref, array_view<InstanceInfo> instances, CacheType& cache) const;

	};
}

template<>
inline std::optional<perfmon::InstanceManager::SortBy> parseEnum<perfmon::InstanceManager::SortBy>(isview name) {
	using SortBy = perfmon::InstanceManager::SortBy;
	if (name == L"None")
		return SortBy::eNONE;
	else if (name == L"InstanceName")
		return SortBy::eINSTANCE_NAME;
	else if (name == L"RawCounter")
		return SortBy::eRAW_COUNTER;
	else if (name == L"FormattedCounter")
		return SortBy::eFORMATTED_COUNTER;
	else if (name == L"Expression")
		return SortBy::eEXPRESSION;
	else if (name == L"RollupExpression")
		return SortBy::eROLLUP_EXPRESSION;
	else if (name == L"Count")
		return SortBy::eCOUNT;

	return {};
}

template<>
inline std::optional<perfmon::InstanceManager::SortOrder> parseEnum<perfmon::InstanceManager::SortOrder>(isview name) {
	using SortOrder = perfmon::InstanceManager::SortOrder;
	if (name == L"Descending")
		return SortOrder::eDESCENDING;
	else if (name == L"Ascending")
		return SortOrder::eASCENDING;

	return {};
}
