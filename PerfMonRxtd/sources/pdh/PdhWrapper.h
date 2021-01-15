/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <Pdh.h>

#include "PdhSnapshot.h"
#include "RainmeterWrappers.h"

namespace rxtd::perfmon::pdh {
	class PdhWrapper : MovableOnlyBase {
		class QueryWrapper : MovableOnlyBase {
			PDH_HQUERY handler = nullptr;

		public:
			QueryWrapper() = default;

			~QueryWrapper() {
				if (handler != nullptr) {
					const PDH_STATUS pdhStatus = PdhCloseQuery(handler);
					if (pdhStatus != ERROR_SUCCESS) {
						// WTF
					}
					handler = nullptr;
				}
			}

			QueryWrapper(QueryWrapper&& other) noexcept : handler(other.handler) {
				other.handler = nullptr;
			}

			QueryWrapper& operator=(QueryWrapper&& other) noexcept {
				if (this == &other)
					return *this;

				handler = other.handler;
				other.handler = nullptr;

				return *this;
			}

			[[nodiscard]]
			PDH_HQUERY& get() {
				return handler;
			}

			[[nodiscard]]
			PDH_HQUERY* getPointer() {
				return &handler;
			}

			[[nodiscard]]
			bool isValid() const {
				return handler != nullptr;
			}
		};

		utils::Rainmeter::Logger log;

		QueryWrapper query;

		bool needFetchExtraIDs = false;

		std::vector<PDH_HCOUNTER> counterHandlers;

		PDH_HCOUNTER idCounterHandler = nullptr;

	public:
		PdhWrapper() = default;

		explicit PdhWrapper(utils::Rainmeter::Logger _log, const string& objectName, const utils::OptionList& counterTokens);

		[[nodiscard]]
		bool isValid() const {
			return query.isValid();
		}

		/**
		 * @returns false if error occurred, true otherwise
		 */
		[[nodiscard]]
		bool fetch(PdhSnapshot& snapshot, PdhSnapshot& idSnapshot);

		[[nodiscard]]
		index getCountersCount() const {
			return counterHandlers.size();
		}

		double extractFormattedValue(index counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const;
	};
}
