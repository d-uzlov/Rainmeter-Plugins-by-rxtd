/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <Pdh.h>
#include <vector>
#include "RainmeterWrappers.h"
#include "OptionParser.h"

#undef UNIQUE_NAME

namespace rxpm::pdh {
	class Snapshot {
		size_t itemsCount = 0;
		size_t counterBufferSize = 0;
		unsigned countersCount = 0;
		std::vector<std::byte> buffer;

	public:
		Snapshot();
		~Snapshot();

		Snapshot(Snapshot&& other) noexcept;
		Snapshot& operator=(Snapshot&& other) noexcept;

		Snapshot(const Snapshot& other) = delete;
		Snapshot& operator=(const Snapshot& other) = delete;

		size_t getItemsCount() const;

		unsigned getCountersCount() const;

		void clear();

		bool isEmpty() const;

		void setCountersCount(unsigned value);

		void setBufferSize(size_t size, size_t items);

		void updateSize();

		PDH_RAW_COUNTER_ITEM_W* getCounterPointer(unsigned counter);

		const PDH_RAW_COUNTER_ITEM_W* getCounterPointer(unsigned counter) const;

		const PDH_RAW_COUNTER& getItem(unsigned counter, size_t index) const;

		const wchar_t* getName(size_t index) const;

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

		rxu::Rainmeter::Logger log;

		QueryWrapper query;

		bool needFetchExtraIDs = false;

		std::vector<PDH_HCOUNTER> counterHandlers;
		uint32_t itemsCount = 0;

		PDH_HCOUNTER idCounterHandler = nullptr;

	public:
		PdhWrapper() = default;
		~PdhWrapper() = default;

		explicit PdhWrapper(rxu::Rainmeter::Logger _log, std::wstring objectName, rxu::OptionParser::OptionList counterTokens);

		PdhWrapper(PdhWrapper&& other) noexcept = default;
		PdhWrapper& operator=(PdhWrapper&& other) noexcept = default;

		PdhWrapper(const PdhWrapper& other) = delete;
		PdhWrapper& operator=(const PdhWrapper& other) = delete;

		bool isValid() const;

		/**
		 * @returns false if error occurred, true otherwise
		 */
		bool fetch(Snapshot& snapshot, Snapshot& idSnapshot);

		size_t getCountersCount() const;

		uint32_t getItemsCount() const;

		double extractFormattedValue(unsigned counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const;
	};
}
