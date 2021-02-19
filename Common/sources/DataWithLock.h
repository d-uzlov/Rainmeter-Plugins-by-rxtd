/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <mutex>

namespace rxtd {
	class DataWithLock {
		bool useLocking = false;
		std::mutex mutex;

	public:
		DataWithLock() = default;

		explicit DataWithLock(bool useLocking) : useLocking(useLocking) { }

		void setUseLocking(bool value) {
			useLocking = value;
		}

		std::unique_lock<std::mutex> getLock() {
			auto lock = std::unique_lock<std::mutex>{ mutex, std::defer_lock };
			if (useLocking) {
				lock.lock();
			}
			return lock;
		}

		template<typename Callable>
		auto runGuarded(Callable callable) {
			auto lock = getLock();
			return callable();
		}
	};
}
