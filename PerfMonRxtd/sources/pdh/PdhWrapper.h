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
	class Snapshot {
		index itemsCount = 0;
		index counterBufferSize = 0;
		index countersCount = 0;
		std::vector<std::byte> buffer;

	public:
		Snapshot();
		~Snapshot();

		Snapshot(Snapshot&& other) noexcept;
		Snapshot& operator=(Snapshot&& other) noexcept;

		Snapshot(const Snapshot& other) = delete;
		Snapshot& operator=(const Snapshot& other) = delete;

		index getItemsCount() const;

		index getCountersCount() const;

		void clear();

		bool isEmpty() const;

		void setCountersCount(unsigned value);

		void setBufferSize(index size, index items);

		void updateSize();

		PDH_RAW_COUNTER_ITEM_W* getCounterPointer(unsigned counter);

		const PDH_RAW_COUNTER_ITEM_W* getCounterPointer(unsigned counter) const;

		const PDH_RAW_COUNTER& getItem(index counter, index index) const;

		const wchar_t* getName(index index) const;

		/**
		 * in wchar_t
		 */
		size_t getNamesSize() const;
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
		index itemsCount = 0;

		PDH_HCOUNTER idCounterHandler = nullptr;

	public:
		PdhWrapper() = default;
		~PdhWrapper() = default;

		explicit PdhWrapper(utils::Rainmeter::Logger _log, string objectName, utils::OptionParser::OptionList counterTokens);

		PdhWrapper(PdhWrapper&& other) noexcept = default;
		PdhWrapper& operator=(PdhWrapper&& other) noexcept = default;

		PdhWrapper(const PdhWrapper& other) = delete;
		PdhWrapper& operator=(const PdhWrapper& other) = delete;

		bool isValid() const;

		/**
		 * @returns false if error occurred, true otherwise
		 */
		bool fetch(Snapshot& snapshot, Snapshot& idSnapshot);

		index getCountersCount() const;

		index getItemsCount() const;

		double extractFormattedValue(index counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const;
	};
}
