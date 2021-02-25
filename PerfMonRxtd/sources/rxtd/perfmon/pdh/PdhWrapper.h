﻿/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "my-pdh.h"

#include "PdhFormat.h"
#include "PdhSnapshot.h"
#include "rxtd/option-parsing/OptionList.h"
#include "rxtd/rainmeter/Logger.h"

namespace rxtd::perfmon::pdh {
	class PdhException : std::runtime_error {
		PdhReturnCode code;
		sview cause;
	public:
		explicit PdhException(PDH_STATUS code, sview cause) : runtime_error("PDH call failed"), code(code), cause(cause) {}

		[[nodiscard]]
		auto getCode() const {
			return code;
		}

		[[nodiscard]]
		auto getCause() const {
			return cause;
		}
	};

	class PdhWrapper : MovableOnlyBase {
		using Logger = common::rainmeter::Logger;
		using OptionList = common::options::OptionList;
		
		struct QueryWrapper : MovableOnlyBase {
			PDH_HQUERY handle = nullptr;

			QueryWrapper() = default;

			~QueryWrapper() {
				release();
			}

			QueryWrapper(QueryWrapper&& other) noexcept {
				release();
				handle = other.handle;
				other.handle = nullptr;
			}

			QueryWrapper& operator=(QueryWrapper&& other) noexcept {
				if (this == &other)
					return *this;

				release();
				handle = other.handle;
				other.handle = nullptr;
				return *this;
			}

			void release() {
				if (handle != nullptr) {
					PdhCloseQuery(handle);
					handle = nullptr;
				}
			}
		};

		Logger log;

		QueryWrapper query;

		bool needFetchExtraIDs = false;

		std::vector<PDH_HCOUNTER> counterHandlers;
		PdhSnapshot mainSnapshot;

		PDH_HCOUNTER processIdCounter = nullptr;
		PdhSnapshot processIdSnapshot;

	public:
		[[nodiscard]]
		// returns true on success, false on error
		bool init(Logger logger);

		// returns true on success, false on error
		[[nodiscard]]
		bool setCounters(sview objectName, const OptionList& counterList, bool gpuExtraIds);

	private:
		[[nodiscard]]
		// throws PdhException on any error
		PDH_HCOUNTER addCounter(sview objectName, sview counterName);

	public:
		[[nodiscard]]
		PdhSnapshot& getMainSnapshot() {
			return mainSnapshot;
		}

		[[nodiscard]]
		PdhSnapshot& getProcessIdsSnapshot() {
			return processIdSnapshot;
		}

		// returns true on success, false on error
		[[nodiscard]]
		bool fetch();

		[[nodiscard]]
		double extractFormattedValue(index counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const;

	private:
		bool fetchSnapshot(array_span<PDH_HCOUNTER> counters, PdhSnapshot& snapshot);
	};
}