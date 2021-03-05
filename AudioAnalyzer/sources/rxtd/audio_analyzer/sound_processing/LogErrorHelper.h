// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "rxtd/Logger.h"

namespace rxtd::audio_analyzer {
	template<typename... CacheTypes>
	class LogErrorHelper {
	public:
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
