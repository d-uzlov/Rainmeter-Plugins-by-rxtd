/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "BufferPrinter.h"
#include "DataHandle.h"
#include "InstanceKeeper.h"

namespace rxtd::common::rainmeter {
	//
	// Basic wrapper over Rainmeter logging functions.
	// Supports formatting using BufferPrinter.
	// Can hold a default prefix for messages.
	// Uses async logging (see InstanceKeeper.h for details).
	// If logging is not needed, logger can be forcefully silenced.
	//
	class Logger {
		DataHandle dataHandle;
		InstanceKeeper instanceKeeper;
		string prefix{};
		bool isSilent = false;

		// Duplicates enum in RainmeterAPI.h because I don't want to include this file
		enum class LogLevel {
			eERROR = 1,
			eWARNING = 2,
			eNOTICE = 3,
			eDEBUG = 4
		};

		mutable buffer_printer::BufferPrinter printer;

	public:
		Logger() = default;

		explicit Logger(DataHandle dh, string prefix = {}) : dataHandle(dh), instanceKeeper(dh), prefix(std::move(prefix)) { }
		
		template<typename... Args>
		void error(const wchar_t* formatString, const Args&... args) const {
			log(LogLevel::eERROR, formatString, args...);
		}

		template<typename... Args>
		void warning(const wchar_t* formatString, const Args&... args) const {
			log(LogLevel::eWARNING, formatString, args...);
		}

		template<typename... Args>
		void notice(const wchar_t* formatString, const Args&... args) const {
			log(LogLevel::eNOTICE, formatString, args...);
		}

		template<typename... Args>
		void debug(const wchar_t* formatString, const Args&... args) const {
			log(LogLevel::eDEBUG, formatString, args...);
		}

		template<typename... Args>
		[[nodiscard]]
		Logger context(const wchar_t* formatString, const Args&... args) const {
			printer.print(formatString, args...);
			string newPrefix = prefix + printer.getBufferPtr();
			printer.reset();

			return Logger{ dataHandle, std::move(newPrefix) };
		}

		void setSilent(bool value) {
			isSilent = value;
		}
		
		[[nodiscard]]
		static Logger getSilent() {
			Logger result{};
			result.setSilent(true);
			return result;
		}

		static void sourcelessLog(const wchar_t* message) {
			InstanceKeeper::sendLog({}, message, static_cast<int>(LogLevel::eDEBUG));
		}

	private:
		template<typename... Args>
		void log(LogLevel logLevel, const wchar_t* formatString, const Args&... args) const {
			if (isSilent) {
				return;
			}

			printer.print(prefix.c_str());

			printer.append(formatString, args...);

			logRainmeter(logLevel, printer.getBufferView());

			printer.reset();
		}

		void logRainmeter(LogLevel logLevel, sview message) const {
			InstanceKeeper::sendLog(dataHandle, message % own(), static_cast<int>(logLevel));
		}
	};
}
