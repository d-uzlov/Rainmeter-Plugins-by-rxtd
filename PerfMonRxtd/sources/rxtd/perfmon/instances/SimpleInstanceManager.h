// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "SortInfo.h"
#include "rxtd/perfmon/BlacklistManager.h"
#include "rxtd/perfmon/Reference.h"
#include "rxtd/perfmon/pdh/NamesManager.h"
#include "rxtd/perfmon/pdh/PdhWrapper.h"

namespace rxtd::perfmon {
	namespace expressions {
		class SimpleExpressionSolver;
	}

	class SimpleInstanceManager {
	public:
		struct Options {
			bool keepDiscarded = false;
			bool syncRawFormatted = true;
			bool limitIndexOffset = false;

			SortInfo sortInfo;

			string blacklist;
			string blacklistOrig;
			string whitelist;
			string whitelistOrig;
		};

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

	private:
		Logger log;

		const pdh::PdhWrapper& pdhWrapper;

		Options options;
		index indexOffset = 0;

		std::vector<InstanceInfo> instances;
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
			std::map<CacheKey, std::optional<const InstanceInfo*>> discarded;

			void reset() {
				simple.clear();
				discarded.clear();
			}
		};

		mutable Caches nameCaches;

	public:
		SimpleInstanceManager(Logger log, const pdh::PdhWrapper& phWrapper);

		void setOptions(Options value) {
			options = value;

			if (options.limitIndexOffset && indexOffset < 0) {
				indexOffset = 0;
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

		index getCountersCount() const {
			return snapshotCurrent.getCountersCount();
		}

		const pdh::ModifiedNameItem& getNames(index ind) const {
			return namesManager.get(ind);
		}

		const pdh::UniqueInstanceId& getIds(index ind) const {
			return idsCurrent[static_cast<size_t>(ind)];
		}

		void update();

		void sort(const expressions::SimpleExpressionSolver& simpleExpressionSolver);

		const pdh::NamesManager& getNamesManager() const {
			return namesManager;
		}

		array_view<InstanceInfo> getInstances() const {
			return instances;
		}

		array_view<InstanceInfo> getDiscarded() const {
			return instancesDiscarded;
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

		const InstanceInfo* findSimpleInstanceByName(const Reference& ref) const;

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

		index findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const;
	};
}
