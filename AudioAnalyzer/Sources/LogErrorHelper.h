/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rainmeter/Logger.h"

namespace rxtd::audio_analyzer {
	template<typename... CacheTypes>
	class LogErrorHelper {
	public:
		using Logger = ::rxtd::common::rainmeter::Logger;
		using KeyType = std::tuple<CacheTypes...>;
		using LogFunctionType = void(*)(Logger& logger, CacheTypes ... args);

	private:
		mutable Logger logger;
		LogFunctionType fun = nullptr;

		mutable std::map<KeyType, bool> cache;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void setLogFunction(LogFunctionType ptr) {
			fun = ptr;
		}

		void reset() {
			cache = {};
		}

		template<typename... Args>
		void log(Args ... args) const {
			auto& logged = cache[KeyType(args...)];
			if (logged) {
				return;
			}

			if (fun != nullptr) {
				fun(logger, CacheTypes(args)...);
			}
			logged = true;
		}
	};

	class NoArgLogErrorHelper {
		using Logger = ::rxtd::common::rainmeter::Logger;
		mutable Logger logger;

		mutable std::map<string, bool> cache;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void reset() {
			cache = {};
		}

		void log(const wchar_t* message) const {
			auto& logged = cache[message];
			if (logged) {
				return;
			}

			// todo make log functions accept strings
			logger.error(message);
			logged = true;
		}
	};
}
