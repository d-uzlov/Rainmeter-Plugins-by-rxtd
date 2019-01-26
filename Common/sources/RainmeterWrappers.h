/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <string_view>
#include <optional>
#include <string>
#include "BufferPrinter.h"

#undef ERROR
#undef WARNING
#undef NOTICE
#undef DEBUG

namespace rxu {
	using namespace std::literals::string_view_literals;

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
				log(LEVEL::ERROR, formatString, args...);
			}
			template<typename... Args>
			void warning(const wchar_t *formatString, const Args&... args) {
				log(LEVEL::WARNING, formatString, args...);
			}
			template<typename... Args>
			void notice(const wchar_t *formatString, const Args&... args) {
				log(LEVEL::NOTICE, formatString, args...);
			}
			template<typename... Args>
			void debug(const wchar_t *formatString, const Args&... args) {
				log(LEVEL::DEBUG, formatString, args...);
			}

		private:
			enum class LEVEL : int {
				ERROR = 1,
				WARNING = 2,
				NOTICE = 3,
				DEBUG = 4
			};

			template<typename... Args>
			void log(LEVEL logLevel, const wchar_t *formatString, const Args&... args) {
				printer.append(formatString, args...);

				log(logLevel, printer.getBufferPtr());

				printer.reset();
			}

			void log(LEVEL logLevel, const wchar_t *string);
		};
		class ContextLogger {
			Logger& log;
			std::wstring prefix { };

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
			void *skin { };

		public:
			Skin() = default;
			explicit Skin(void* skin);

			void* getRawPointer() const;
		};

	private:
		void *rm { };
		Skin skin;
		std::wstring measureName;
		Logger logger;
		mutable std::wstring optionNameBuffer;

	public:
		Rainmeter();
		explicit Rainmeter(void *rm);
		~Rainmeter() = default;

		Rainmeter(Rainmeter&& other) noexcept;
		Rainmeter(const Rainmeter& other);
		Rainmeter& operator=(const Rainmeter& other);
		Rainmeter& operator=(Rainmeter&& other) noexcept;

		const wchar_t* readString(std::wstring_view optionName, const wchar_t* defaultValue = L"") const;
		const wchar_t* readPath(std::wstring_view optionName, const wchar_t* defaultValue = L"") const;
		double readDouble(std::wstring_view optionName, double defaultValue = 0.0) const;

		int64_t readInt(std::wstring_view optionName, int64_t defaultValue = 0) const {
			return static_cast<int64_t>(readDouble(optionName, static_cast<double>(defaultValue)));
		}

		bool readBool(std::wstring_view optionName, bool defaultValue = false) const {
			return readInt(optionName, defaultValue ? 1 : 0) != 0;
		}

		const wchar_t* replaceVariables(std::wstring_view string) const;
		const wchar_t* transformPathToAbsolute(std::wstring_view path) const;

		void executeCommand(std::wstring_view command) {
			executeCommand(command, skin);
		}

		void executeCommand(std::wstring_view command, Skin skin);

		Logger& getLogger();

		void* getRawPointer();
		Skin getSkin() const;
		const std::wstring& getMeasureName() const;
		void* getWindowHandle();

	private:
		const wchar_t* makeNullTerminated(std::wstring_view view) const;
	};
}
