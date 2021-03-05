// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

#include "my-pdh.h"

#include "PdhFormat.h"
#include "PdhSnapshot.h"
#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/Logger.h"

namespace rxtd::perfmon::pdh {
	class PdhException : public std::runtime_error {
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
		using OptionList = option_parsing::OptionList;
		
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
