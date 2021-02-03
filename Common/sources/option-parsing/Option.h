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

namespace rxtd::utils {
	class OptionList;
	class OptionMap;
	class OptionSequence;

	// Class, that allows you to parse strings as some options.
	class Option : public OptionBase<Option> {
	public:
		Option() = default;

		explicit Option(sview view) : OptionBase(view) { }

		explicit Option(isview view) : Option(view % csView()) { }

		explicit Option(const wchar_t* view) : Option(sview{ view }) {
			own();
		}

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
			return float(asFloat(double(defaultValue)));
		}

		// Parse integer value, support math operations.
		template<typename IntType = int32_t>
		typename std::enable_if<std::is_integral<IntType>::value, IntType>::type
		asInt(IntType defaultValue = 0) const {
			const auto dVal = asFloat(static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<IntType>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<IntType>::lowest())) {
				return defaultValue;
			}
			return static_cast<IntType>(dVal);
		}

		[[nodiscard]]
		bool asBool(bool defaultValue = false) const;

		[[nodiscard]]
		std::pair<Option, Option> breakFirst(wchar_t separator) const;

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

	// same as option but doesn't allocate memory in #asString when the object it r-value
	// intended to be created when reading options from already allocated map/list
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
