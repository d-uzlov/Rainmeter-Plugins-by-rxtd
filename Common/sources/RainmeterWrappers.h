/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "BufferPrinter.h"
#include "OptionParser.h"

namespace rxtd::utils {
	class Rainmeter {
	public:
		class Logger {
			void *rm { };

		public:
			BufferPrinter printer;

			Logger() = default;
			explicit Logger(void* rm);
			~Logger() = default;

			Logger(Logger&& other) noexcept;
			Logger(const Logger& other);
			Logger& operator=(const Logger& other);
			Logger& operator=(Logger&& other) noexcept;

			template<typename... Args>
			void error(const wchar_t *formatString, const Args&... args) {
				log(LogLevel::eERROR, formatString, args...);
			}
			template<typename... Args>
			void warning(const wchar_t *formatString, const Args&... args) {
				log(LogLevel::eWARNING, formatString, args...);
			}
			template<typename... Args>
			void notice(const wchar_t *formatString, const Args&... args) {
				log(LogLevel::eNOTICE, formatString, args...);
			}
			template<typename... Args>
			void debug(const wchar_t *formatString, const Args&... args) {
				log(LogLevel::eDEBUG, formatString, args...);
			}

		private:
			enum class LogLevel {
				eERROR = 1,
				eWARNING = 2,
				eNOTICE = 3,
				eDEBUG = 4
			};

			template<typename... Args>
			void log(LogLevel logLevel, const wchar_t *formatString, const Args&... args) {
				printer.append(formatString, args...);

				log(logLevel, printer.getBufferPtr());

				printer.reset();
			}

			void log(LogLevel logLevel, const wchar_t *string);
		};
		class ContextLogger {
			Logger& log;
			string prefix { };

		public:
			ContextLogger(Logger &logger) : log(logger) { }

			template<typename... Args>
			void setPrefix(const wchar_t *formatString, const Args&... args) {
				log.printer.print(formatString, args...);
				prefix = log.printer.getBufferPtr();
				log.printer.reset();
			}

			template<typename... Args>
			void error(const wchar_t *formatString, const Args&... args) const {
				printInfo();
				log.error(formatString, args...);
			}
			template<typename... Args>
			void warning(const wchar_t *formatString, const Args&... args) const {
				printInfo();
				log.warning(formatString, args...);
			}
			template<typename... Args>
			void notice(const wchar_t *formatString, const Args&... args) const {
				printInfo();
				log.notice(formatString, args...);
			}
			template<typename... Args>
			void debug(const wchar_t *formatString, const Args&... args) const {
				printInfo();
				log.debug(formatString, args...);
			}

		private:
			void printInfo() const {
				log.printer.print(prefix.c_str());
			}
		};

		/**
		 * Wrapper over a rainmeter skin handler.
		 * Used for type safety only.
		 */
		class Skin {
			friend Rainmeter;

			void *skin { };

		public:
			Skin() = default;

			bool operator<(Skin other) const;

		private:
			explicit Skin(void* skin);

			void* getRawPointer() const;
		};

	private:
		void *rm { };
		Skin skin;
		string measureName;
		Logger logger;
		mutable string optionNameBuffer;

	public:
		Rainmeter() = default;
		explicit Rainmeter(void *rm);
		~Rainmeter() = default;

		Rainmeter(const Rainmeter& other) = default;
		Rainmeter(Rainmeter&& other) noexcept = default;
		Rainmeter& operator=(const Rainmeter& other) = default;
		Rainmeter& operator=(Rainmeter&& other) noexcept = default;

		Option readOption(sview optionName) const;
		sview readString(sview optionName, const wchar_t* defaultValue = L"") const;
		sview readPath(sview optionName, const wchar_t* defaultValue = L"") const;
		double readDouble(sview optionName, double defaultValue = 0.0) const;

		template<typename I = int32_t>
		typename std::enable_if<std::is_integral<I>::value, I>::type
		readInt(sview optionName, I defaultValue = 0) const {
			const auto dVal = readDouble(optionName, static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<I>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<I>::lowest())) {
				return defaultValue;
			}
			return static_cast<I>(dVal);
		}

		bool readBool(sview optionName, bool defaultValue = false) const {
			return readInt(optionName, defaultValue ? 1 : 0) != 0;
		}

		sview replaceVariables(sview string) const;
		sview transformPathToAbsolute(sview path) const;

		void executeCommand(sview command) {
			executeCommand(command, skin);
		}

		void executeCommand(sview command, Skin skin);

		Logger& getLogger();

		void* getRawPointer();
		Skin getSkin() const;
		const string& getMeasureName() const;
		void* getWindowHandle();

	private:
		const wchar_t* makeNullTerminated(sview view) const;
	};
}
