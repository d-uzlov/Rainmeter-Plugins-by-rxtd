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
		double sortValue = 0.0;
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

		struct Options {
			bool keepDiscarded = false;
			bool syncRawFormatted = true;
			bool rollup = false;
			bool limitIndexOffset = false;

			SortBy sortBy = SortBy::eNONE;
			index sortIndex = 0;
			SortOrder sortOrder = SortOrder::eDESCENDING;
			RollupFunction sortRollupFunction = RollupFunction::eSUM;
		};

	private:
		utils::Rainmeter::Logger& log;

		const pdh::PdhWrapper& pdhWrapper;

		Options options;
		index indexOffset = 0;

		std::vector<InstanceInfo> instances;
		std::vector<InstanceInfo> instancesRolledUp;
		std::vector<InstanceInfo> instancesDiscarded;

		const pdh::PdhSnapshot& snapshotCurrent;
		const pdh::PdhSnapshot& snapshotPrevious;
		const BlacklistManager& blacklistManager;

		std::vector<pdh::UniqueInstanceId> idsPrevious;
		std::vector<pdh::UniqueInstanceId> idsCurrent;
		pdh::NamesManager namesManager;

		// (use orig name, partial match, name) -> instanceInfo
		mutable std::map<std::tuple<bool, bool, sview>, std::optional<const InstanceInfo*>> nameCache;
		mutable decltype(nameCache) nameCacheRollup;
		mutable decltype(nameCache) nameCacheDiscarded;

	public:
		InstanceManager(
			utils::Rainmeter::Logger& log, const pdh::PdhWrapper& phWrapper,
			const pdh::PdhSnapshot& snapshotCurrent, const pdh::PdhSnapshot& snapshotPrevious,
			const BlacklistManager& blacklistManager
		);

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


	private:
		void buildInstanceKeysZero();

		void buildInstanceKeys();

		void buildRollupKeys();

		index findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const;

		const InstanceInfo* findInstanceByNameInList(
			const Reference& ref,
			const std::vector<InstanceInfo>& instances, std::map<std::tuple<bool, bool, sview>,
			                                                     std::optional<const InstanceInfo*>>& cache
		) const;

	};
}
