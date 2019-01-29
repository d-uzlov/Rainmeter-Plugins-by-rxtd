/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "my-windows.h"
#include <Pdh.h>
#include "RainmeterWrappers.h"
#include "OptionParser.h"

#undef UNIQUE_NAME

namespace rxtd::perfmon::pdh {
	using counter_t = int16_t; // because expressions?
	using item_t = int16_t;

	class Snapshot {
		std::vector<std::byte> buffer;
		index counterBufferSize = 0;
		item_t itemsCount = 0;
		counter_t countersCount = 0;

	public:
		Snapshot();
		~Snapshot();

		Snapshot(Snapshot&& other) noexcept;
		Snapshot& operator=(Snapshot&& other) noexcept;

		Snapshot(const Snapshot& other) = delete;
		Snapshot& operator=(const Snapshot& other) = delete;

		item_t getItemsCount() const;

		counter_t getCountersCount() const;

		void clear();

		bool isEmpty() const;

		void setCountersCount(counter_t value);

		void setBufferSize(index size, item_t items);

		void updateSize();

		PDH_RAW_COUNTER_ITEM_W* getCounterPointer(counter_t counter);

		const PDH_RAW_COUNTER_ITEM_W* getCounterPointer(counter_t counter) const;

		const PDH_RAW_COUNTER& getItem(counter_t counter, item_t item) const;

		const wchar_t* getName(item_t item) const;

		/**
		 * in wchar_t
		 */
		index getNamesSize() const;
	};

	class PdhWrapper {
		class QueryWrapper {
			PDH_HQUERY handler = nullptr;

		public:
			QueryWrapper() = default;
			~QueryWrapper();

			QueryWrapper(const QueryWrapper& other) = delete;
			QueryWrapper(QueryWrapper&& other) noexcept;
			QueryWrapper& operator=(const QueryWrapper& other) = delete;
			QueryWrapper& operator=(QueryWrapper&& other) noexcept;

			PDH_HQUERY& get();
			PDH_HQUERY* getPointer();

			bool isValid() const;
		};

		utils::Rainmeter::Logger log;

		QueryWrapper query;

		bool needFetchExtraIDs = false;

		std::vector<PDH_HCOUNTER> counterHandlers;

		PDH_HCOUNTER idCounterHandler = nullptr;

	public:
		PdhWrapper() = default;
		~PdhWrapper() = default;

		explicit PdhWrapper(utils::Rainmeter::Logger _log, const string& objectName, const utils::OptionParser::OptionList
		                    & counterTokens);

		PdhWrapper(PdhWrapper&& other) noexcept = default;
		PdhWrapper& operator=(PdhWrapper&& other) noexcept = default;

		PdhWrapper(const PdhWrapper& other) = delete;
		PdhWrapper& operator=(const PdhWrapper& other) = delete;

		bool isValid() const;

		/**
		 * @returns false if error occurred, true otherwise
		 */
		bool fetch(Snapshot& snapshot, Snapshot& idSnapshot);

		counter_t getCountersCount() const;

		double extractFormattedValue(counter_t counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const;
	};
}
