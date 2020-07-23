/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Color.h"
#include "StringUtils.h"
#include "AbstractOption.h"

namespace rxtd::utils {
	class OptionList;
	class OptionMap;
	class OptionSequence;
	struct OptionSeparated;

	// Class, that allows you to parse options.
	class Option : public AbstractOption<Option> {
	public:
		Option() = default;
		explicit Option(sview view);
		explicit Option(wchar_t* view);

		// Raw view of option.
		sview asString(sview defaultValue = { }) const &;
		// Raw view of option.
		string asString(sview defaultValue = { }) && {
			return string{ asString(defaultValue) };
		}

		// Raw case-insensitive view of option.
		isview asIString(isview defaultValue = { }) const &;
		// Raw case-insensitive view of option.
		istring asIString(isview defaultValue = { }) && {
			return istring { asIString(defaultValue) };
		}
		// Parse float, support math operations.
		double asFloat(double defaultValue = 0.0) const;

		// Parse integer value, support math operations.
		template <typename IntType = int32_t>
		typename std::enable_if<std::is_integral<IntType>::value, IntType>::type
		asInt(IntType defaultValue = 0) const {
			const auto dVal = asFloat(static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<IntType>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<IntType>::lowest())) {
				return defaultValue;
			}
			return static_cast<IntType>(dVal);
		}

		bool asBool(bool defaultValue = false) const;
		// Parse Color, support math operations per color component.
		Color asColor(Color defaultValue = { }) const;

		OptionSeparated breakFirst(wchar_t separator) const;

		OptionMap asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const &;
		OptionMap asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) &&;
		OptionList asList(wchar_t delimiter) const &;
		OptionList asList(wchar_t delimiter) &&;
		OptionSequence asSequence(
			wchar_t optionBegin = L'[', wchar_t optionEnd = L']',
			wchar_t paramDelimiter = L',',
			wchar_t optionDelimiter = L' '
		) const &;
		OptionSequence asSequence(
			wchar_t optionBegin = L'[', wchar_t optionEnd = L']',
			wchar_t paramDelimiter = L',',
			wchar_t optionDelimiter = L' '
		) &&;

		bool empty() const;

	private:
		static double parseNumber(sview source);
		static std::map<SubstringViewInfo, SubstringViewInfo> parseMapParams(
			sview source,
			wchar_t optionDelimiter,
			wchar_t nameDelimiter
		);
	};

	struct OptionSeparated {
		Option first;
		Option rest;

		OptionSeparated() = default;

		OptionSeparated(Option first, Option rest) :
			first(std::move(first)),
			rest(std::move(rest)) {
		}
	};
}
