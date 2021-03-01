/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "OptionBase.h"
#include "rxtd/DataWithLock.h"
#include "rxtd/Logger.h"

namespace rxtd::option_parsing {
	class GhostOption;
	class OptionList;
	class OptionMap;
	class OptionSequence;

	//
	// Class Option allows easy parsing of relatively complex input parameters.
	// See documentation of respective classes to learn about syntax for OptionMap, OptionList, OptionSequence
	//
	class Option : public OptionBase {
		struct LoggerContainer : public DataWithLock {
			Logger logger = Logger::getSilent();
		};

		static LoggerContainer loggerContainer;

	public:
		Option() = default;

		explicit Option(sview view) : OptionBase(view) { }

		explicit Option(isview view) : Option(view % csView()) { }

		explicit Option(const wchar_t* view) : Option(sview{ view }) {}

		static void setLogger(Logger value);

		// Raw view of the option.
		[[nodiscard]]
		sview asString(sview defaultValue = {}) const &;

		// Raw view of the option.
		// Separate case for r-value, makes sure that result won't become invalid after object destruction.
		[[nodiscard]]
		string asString(sview defaultValue = {}) const && {
			return string{ asString(defaultValue) };
		}

		// Raw case-insensitive view of the option.
		[[nodiscard]]
		isview asIString(isview defaultValue = {}) const &;

		// Raw case-insensitive view of the option.
		// Separate case for r-value, makes sure that result won't become invalid after object destruction.
		[[nodiscard]]
		istring asIString(isview defaultValue = {}) const && {
			return istring{ asIString(defaultValue) };
		}

		// Parse float, support math operations.
		[[nodiscard]]
		double asFloat(double defaultValue = 0.0) const;

		// Parse float, support math operations.
		[[nodiscard]]
		float asFloatF(float defaultValue = 0.0) const {
			return static_cast<float>(asFloat(static_cast<double>(defaultValue)));
		}

		// Parse integer value, support math operations.
		template<typename IntType = int32_t>
		IntType asInt(IntType defaultValue = 0) const {
			static_assert(std::is_integral<IntType>::value);

			const auto dVal = asFloat(static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<IntType>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<IntType>::lowest())) {
				return defaultValue;
			}
			return static_cast<IntType>(dVal);
		}

		[[nodiscard]]
		bool asBool(bool defaultValue = false) const;

		// Returns a pair of options,
		// where first Option contains content before first inclusion of separator
		// and second Option contains everything after first inclusion of separator
		[[nodiscard]]
		std::pair<GhostOption, GhostOption> breakFirst(wchar_t separator) const &;

		// See #breakFirst() const &
		// Separate case for r-value, makes sure that result won't become invalid after object destruction.
		[[nodiscard]]
		std::pair<Option, Option> breakFirst(wchar_t separator) &&;

		[[nodiscard]]
		OptionMap asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const &;

		[[nodiscard]]
		OptionMap asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) &&;

		[[nodiscard]]
		OptionList asList(wchar_t delimiter) const &;

		[[nodiscard]]
		OptionList asList(wchar_t delimiter) &&;

		[[nodiscard]]
		OptionSequence asSequence(
			wchar_t optionBegin = L'[', wchar_t optionEnd = L']',
			wchar_t optionDelimiter = L' '
		) const &;

		[[nodiscard]]
		OptionSequence asSequence(
			wchar_t optionBegin = L'[', wchar_t optionEnd = L']',
			wchar_t optionDelimiter = L' '
		) &&;

		[[nodiscard]]
		bool empty() const {
			return getView().empty();
		}

	private:
		[[nodiscard]]
		static double parseNumber(sview source);
	};

	std::wostream& operator<<(std::wostream& stream, const Option& opt);

	// Same as Option but doesn't allocate memory when the object it an r-value.
	// Intended to be created when reading options from already allocated map/list.
	class GhostOption : public Option {
	public:
		GhostOption() = default;

		explicit GhostOption(sview view) : Option(view) { }

		explicit GhostOption(isview view) : Option(view) { }

		explicit GhostOption(wchar_t* view) : Option(view) { }

		[[nodiscard]]
		sview asString(sview defaultValue = {}) const {
			return Option::asString(defaultValue);
		}

		[[nodiscard]]
		isview asIString(isview defaultValue = {}) const {
			return Option::asIString(defaultValue);
		}
	};
}
