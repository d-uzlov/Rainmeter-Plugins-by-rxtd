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
	struct Indices {
		int_fast16_t current{};
		int_fast16_t previous{};
	};

	struct InstanceInfo {
		sview sortName;
		double sortValue = 0.0;
		Indices indices;

		[[nodiscard]]
		Indices getFirst() const {
			return indices;
		}
	};

	struct RollupInstanceInfo {
		std::vector<Indices> indices;
		sview sortName;
		double sortValue = 0.0;

		[[nodiscard]]
		Indices getFirst() const {
			return indices.front();
		}
	};

	class InstanceManager {
	public:
		using Logger = common::rainmeter::Logger;

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
		std::vector<RollupInstanceInfo> instancesRolledUp;
		std::vector<InstanceInfo> instancesDiscarded;

		pdh::PdhSnapshot snapshotCurrent;
		pdh::PdhSnapshot snapshotPrevious;
		pdh::PdhSnapshot processIdsSnapshot;
		BlacklistManager blacklistManager;

		std::vector<pdh::UniqueInstanceId> idsPrevious;
		std::vector<pdh::UniqueInstanceId> idsCurrent;
		pdh::NamesManager namesManager;

		struct CacheKey {
			MatchPattern pattern;
			bool useOriginalName;

			friend bool operator<(const CacheKey& lhs, const CacheKey& rhs) {
				return lhs.pattern < rhs.pattern
					&& lhs.useOriginalName < rhs.useOriginalName;
			}
		};

		struct Caches {
			std::map<CacheKey, std::optional<const InstanceInfo*>> simple;
			std::map<CacheKey, std::optional<const RollupInstanceInfo*>> rollup;
			std::map<CacheKey, std::optional<const InstanceInfo*>> discarded;

			void reset() {
				simple.clear();
				rollup.clear();
				discarded.clear();
			}
		};

		mutable Caches nameCaches;

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

		template<typename Solver, typename InstanceInfo>
		void sort(const Solver& expressionResolver, array_span<InstanceInfo> instances) {
			if (options.sortBy == SortBy::eNONE || instances.empty()) {
				return;
			}

			if (options.sortBy == SortBy::eINSTANCE_NAME) {
				switch (options.sortOrder) {
				case SortOrder::eASCENDING:
					std::sort(
						instances.begin(), instances.end(),
						[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
							return lhs.sortName > rhs.sortName;
						}
					);
					break;
				case SortOrder::eDESCENDING:
					std::sort(
						instances.begin(), instances.end(),
						[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
							return lhs.sortName < rhs.sortName;
						}
					);
					break;
				default:
					log.error(L"unexpected sortOrder {}", options.sortOrder);
					break;
				}
				return;
			}

			Reference ref;
			ref.counter = options.sortIndex;
			ref.rollupFunction = options.sortRollupFunction;

			switch (options.sortBy) {
			case SortBy::eNONE: return;
			case SortBy::eINSTANCE_NAME: return;
			case SortBy::eRAW_COUNTER:
				ref.type = Reference::Type::COUNTER_RAW;
				break;
			case SortBy::eFORMATTED_COUNTER:
				ref.type = Reference::Type::COUNTER_FORMATTED;
				break;
			case SortBy::eEXPRESSION:
				ref.type = Reference::Type::EXPRESSION;
				break;
			case SortBy::eROLLUP_EXPRESSION:
				ref.type = Reference::Type::ROLLUP_EXPRESSION;
				break;
			case SortBy::eCOUNT:
				ref.type = Reference::Type::COUNT;
				break;
			}

			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.resolveReference(ref, instance.indices);
			}

			switch (options.sortOrder) {
			case SortOrder::eASCENDING:
				std::sort(
					instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
						return lhs.sortValue < rhs.sortValue;
					}
				);
				break;
			case SortOrder::eDESCENDING:
				std::sort(
					instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
						return lhs.sortValue > rhs.sortValue;
					}
				);
				break;
			default:
				log.error(L"unexpected sortOrder {}", options.sortOrder);
				break;
			}
		}

		array_span<InstanceInfo> getInstances() {
			return instances;
		}

		array_span<InstanceInfo> getDiscarded() {
			return instancesDiscarded;
		}

		array_span<RollupInstanceInfo> getRollupInstances() {
			return instancesRolledUp;
		}

		array_view<InstanceInfo> getInstances() const {
			return instances;
		}

		array_view<InstanceInfo> getDiscarded() const {
			return instancesDiscarded;
		}

		array_view<RollupInstanceInfo> getRollupInstances() const {
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

		const InstanceInfo* findSimpleInstance(const Reference& ref, index sortedIndex) const;

		const RollupInstanceInfo* findRollupInstance(const Reference& ref, index sortedIndex) const;

		const InstanceInfo* findSimpleInstanceByName(const Reference& ref) const;

		const RollupInstanceInfo* findRollupInstanceByName(const Reference& ref) const;

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

		template<typename InstanceType, typename CacheType>
		const InstanceType* findInstanceByNameInList(const Reference& ref, array_view<InstanceType> instances, CacheType& cache) const {
			auto& itemOpt = cache[{ ref.namePattern, ref.useOrigName }];
			if (itemOpt.has_value()) {
				return itemOpt.value(); // already cached
			}

			if (ref.useOrigName) {
				for (const auto& item : instances) {
					if (ref.namePattern.match(namesManager.get(item.getFirst().current).originalName)) {
						itemOpt = &item;
						return itemOpt.value();
					}
				}
			} else {
				for (const auto& item : instances) {
					if (ref.namePattern.match(item.sortName)) {
						itemOpt = &item;
						return itemOpt.value();
					}
				}
			}

			itemOpt = nullptr;
			return itemOpt.value();
		}
	};
}

template<>
inline std::optional<rxtd::perfmon::InstanceManager::SortBy> parseEnum<rxtd::perfmon::InstanceManager::SortBy>(rxtd::isview name) {
	using SortBy = rxtd::perfmon::InstanceManager::SortBy;
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
inline std::optional<rxtd::perfmon::InstanceManager::SortOrder> parseEnum<rxtd::perfmon::InstanceManager::SortOrder>(rxtd::isview name) {
	using SortOrder = rxtd::perfmon::InstanceManager::SortOrder;
	if (name == L"Descending")
		return SortOrder::eDESCENDING;
	else if (name == L"Ascending")
		return SortOrder::eASCENDING;

	return {};
}
