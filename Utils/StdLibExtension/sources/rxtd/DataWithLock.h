// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

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
