/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Color.h"
#include "StringUtils.h"
#include <optional>
#include <utility>

namespace rxtd::utils {
	class Tokenizer {
		std::vector<SubstringViewInfo> tempList { };

	public:
		std::vector<SubstringViewInfo> parse(sview string, wchar_t delimiter);

	private:
		void emitToken(index begin, index end);

		void tokenize(sview string, wchar_t delimiter);

		void trimSpaces(sview string);
	};

	class OptionList;
	class OptionMap;
	struct OptionSeparated;
	// Class, that allows you to parse options.
	class Option {
		// source may be not used, all operations are performed with view, which can point to other arrays
		sview _view;
		// ↓↓ yes, this must be a vector and not a string,
		// ↓↓ because small string optimization kills string views
		std::vector<wchar_t> source;

	public:
		Option() = default;
		explicit Option(sview view);

		// Raw view of option.
		sview asString(sview defaultValue = { }) const;
		// Raw case-insensitive view of option.
		isview asIString(isview defaultValue = { }) const;
		// Parse float, support math operations.
		double asFloat(double defaultValue = 0.0) const;

		// Parse integer value, support math operations.
		template<typename I = int32_t>
		typename std::enable_if<std::is_integral<I>::value, I>::type
			asInt(I defaultValue = 0) const {
			const auto dVal = asFloat(static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<I>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<I>::lowest())) {
				return defaultValue;
			}
			return static_cast<I>(dVal);
		}
		// Alias to asFloat() != 0
		bool asBool(bool defaultValue = false) const {
			return asFloat(defaultValue ? 1.0 : 0.0) != 0.0;
		}
		// Parse Color, support math operations per color component.
		Color asColor(Color defaultValue = { }) const;

		OptionSeparated breakFirst(wchar_t separator) const;

		OptionMap asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const;
		OptionList asList(wchar_t delimiter) const;

		Option own();

	private:
		static double parseNumber(sview source);
		sview getView() const;
	};

	struct OptionSeparated {
		Option first;
		Option rest;

		OptionSeparated() = default;
		OptionSeparated(Option first, Option rest)
			: first(std::move(first)),
			rest(std::move(rest)) { }
	};

	// List of string.
	class OptionList {
		// source may be not used, all operations are performed with view, which can point to other arrays
		sview _view;
		// ↓↓ yes, this must be a vector and not a string,
		// ↓↓ because small string optimization kills string views
		std::vector<wchar_t> source;

		std::vector<SubstringViewInfo> list;

	public:
		OptionList() = default;

		OptionList(sview view, std::vector<SubstringViewInfo>&& list);

		// Allows you to steal inner resources.
		std::pair<sview, std::vector<SubstringViewInfo>> consume() && ;

		// Count of elements in list.
		index size() const;
		// Alias to "size() == 0".
		bool empty() const;

		// Parseable view of Nth option.
		Option get(index ind) const;

		class iterator {
			const OptionList& container;
			index ind;

		public:
			iterator(const OptionList& container, index _index);

			iterator& operator++();

			bool operator !=(const iterator& other) const;

			Option operator*() const;
		};

		iterator begin() const;

		iterator end() const;

		OptionList own();

	private:
		sview getView() const;
	};

	class OptionMap {
	public:
		struct MapOptionInfo {
			SubstringViewInfo substringInfo { };
			bool touched = false;

			MapOptionInfo() = default;
			MapOptionInfo(SubstringViewInfo substringInfo) : substringInfo(substringInfo) { }
		};

	private:
		// source may be not used, all operations are performed with view, which can point to other arrays
		sview _view;
		// ↓↓ yes, this must be a vector and not a string,
		// ↓↓ because small string optimization kills string views
		std::vector<wchar_t> source;
		// For move and copy operations.
		std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };

		// For fast search.
		mutable std::map<isview, MapOptionInfo> params { };

	public:
		OptionMap();
		OptionMap(sview source, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo);

		//Returns named option, search is case-insensitive.
		// Doesn't raise the "touched" flag on the option
		Option getUntouched(sview name) const;

		//Returns named option, search is case-insensitive.
		Option get(sview name) const;

		//Returns named option, search is case-insensitive.
		Option get(isview name) const;

		//Returns named option, search is case-insensitive.
		// This overload disambiguates sview/isview call.
		Option get(const wchar_t* name) const;

		// returns true if option with such name exists.
		bool has(sview name) const;

		// returns true if option with such name exists.
		bool has(isview name) const;

		// returns true if option with such name exists.
		// This overload disambiguates sview/isview call.
		bool has(const wchar_t* name) const;

		// Allows you to iterate over all available options.
		const std::map<isview, MapOptionInfo>& getParams() const;

		std::vector<isview> getListOfUntouched() const;

		OptionMap own();
	private:
		void fillParams() const;
		MapOptionInfo* find(isview name) const;
		sview getView() const;
	};



	class OptionParser {
	public:
		static Option parse(sview string);
	};
}
