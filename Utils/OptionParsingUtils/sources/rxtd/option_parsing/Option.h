// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "OptionBase.h"
#include "rxtd/DataWithLock.h"

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
	public:
		Option() = default;

		explicit Option(sview view) : OptionBase(view) { }

		explicit Option(isview view) : Option(view % csView()) { }

		explicit Option(const wchar_t* view) : Option(sview{ view }) {}

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
