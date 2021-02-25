/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/rainmeter/Logger.h"

namespace rxtd::audio_analyzer {
	template<typename... CacheTypes>
	class LogErrorHelper {
	public:
		using Logger = common::rainmeter::Logger;
		using KeyType = std::tuple<CacheTypes...>;
		using LogFunctionType = void(*)(Logger& logger, CacheTypes ... args);

	private:
		Logger logger;
		LogFunctionType fun = nullptr;

		std::map<KeyType, bool> cache;

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
		void log(Args ... args) {
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
		using Logger = common::rainmeter::Logger;

		Logger logger;
		std::map<string, bool> cache;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		void reset() {
			cache = {};
		}

		void log(sview message) {
			auto& logged = cache[message % own()];
			if (logged) {
				return;
			}

			logger.error(message);
			logged = true;
		}
	};
}
