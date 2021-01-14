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
#include "option-parser/Option.h"

namespace rxtd::utils {
	class Rainmeter {
	public:
		class InstanceKeeper final {
			bool initialized = false;

		public:
			InstanceKeeper() = default;
			explicit InstanceKeeper(void* rm);

			~InstanceKeeper() {
				deinit();
			}

			InstanceKeeper(const InstanceKeeper& other) = delete;
			InstanceKeeper& operator=(const InstanceKeeper& other) = delete;

			InstanceKeeper(InstanceKeeper&& other) noexcept : initialized(other.initialized) {
				other.initialized = false;
			}

			InstanceKeeper& operator=(InstanceKeeper&& other) noexcept {
				if (this == &other)
					return *this;
				deinit();
				initialized = std::exchange(other.initialized, false);
				return *this;
			}

		private:
			void deinit();
		};

		class Logger {
			friend Rainmeter;

			bool isSilent = false;
			void* rm{};
			string prefix{};

		public:
			enum class LogLevel {
				eERROR = 1,
				eWARNING = 2,
				eNOTICE = 3,
				eDEBUG = 4
			};

			mutable BufferPrinter printer;

			Logger() = default;

		private:
			Logger(void* rm, string prefix) : rm(rm), prefix(std::move(prefix)) { }

		public:
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
				if (isSilent) {
					return *this;
				}

				printer.print(formatString, args...);
				string newPrefix = prefix + printer.getBufferPtr();
				printer.reset();

				return { rm, std::move(newPrefix) };
			}

			[[nodiscard]]
			static Logger silent() {
				Logger result{};
				result.isSilent = true;
				return result;
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

			void logRainmeter(LogLevel logLevel, sview message) const;
		};

		/**
		 * Wrapper over a rainmeter skin handler.
		 * Used for type safety only.
		 */
		class Skin {
			friend Rainmeter;

			void* skin{};

		public:
			Skin() = default;

			[[nodiscard]]
			bool operator<(Skin other) const {
				return skin < other.skin;
			}

		private:
			explicit Skin(void* skin) : skin(skin) { }

			[[nodiscard]]
			void* getRawPointer() const {
				return skin;
			}
		};

	private:
		void* rm{};
		Skin skin{};
		string measureName;
		mutable string optionNameBuffer;

	public:
		Rainmeter() = default;
		explicit Rainmeter(void* rm);

		[[nodiscard]]
		Option read(sview optionName, bool replaceVariables = true) const {
			return Option{ readString(optionName, L"", replaceVariables) }.own();
		}

		[[nodiscard]]
		sview readString(sview optionName, const wchar_t* defaultValue = L"", bool replaceVariables = true) const;
		[[nodiscard]]
		sview readPath(sview optionName, const wchar_t* defaultValue = L"") const;
		[[nodiscard]]
		double readDouble(sview optionName, double defaultValue = 0.0) const;

		[[nodiscard]]
		sview replaceVariables(sview string) const;
		[[nodiscard]]
		sview transformPathToAbsolute(sview path) const;

		void executeCommandAsync(sview command) {
			if (command.empty()) {
				return;
			}
			executeCommandAsync(command, skin);
		}

		void executeCommandAsync(sview command, Skin skin);

		[[nodiscard]]
		Logger createLogger() const {
			return { rm, {} };
		}

		[[nodiscard]]
		Skin getSkin() const {
			return skin;
		}

		[[nodiscard]]
		sview getMeasureName() const {
			return measureName;
		}

		[[nodiscard]]
		void* getWindowHandle();

		static void sourcelessLog(const wchar_t* message);

		InstanceKeeper getInstanceKeeper();

	private:
		[[nodiscard]]
		const wchar_t* makeNullTerminated(sview view) const;
	};
}
