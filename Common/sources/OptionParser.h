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

namespace rxtd::utils {
	class Tokenizer {
	public:
		static std::vector<SubstringViewInfo> parse(sview view, wchar_t delimiter);
		static std::vector<std::vector<SubstringViewInfo>> parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter);

	private:
		static void emitToken(std::vector<SubstringViewInfo>& list, index begin, index end);

		static void tokenize(std::vector<SubstringViewInfo>& list, sview string, wchar_t delimiter);

		static void trimSpaces(std::vector<SubstringViewInfo>& list, sview string);
	};

	template<typename T>
	class AbstractOption {
		// View of string containing data for this option
		// There are a view and a source, because source may be empty while view points to data in some other object
		// It's a user's responsibility to manage memory for this
		sview _view;
		// ↓↓ yes, this must be a vector and not a string,
		// ↓↓ because small string optimization kills string views on move
		std::vector<wchar_t> source;

	public:
		explicit AbstractOption() = default;
		AbstractOption(sview view) : _view(view) { }

		T& own() {
			source.resize(_view.size());
			std::copy(_view.begin(), _view.end(), source.begin());

			return static_cast<T&>(*this);
		}

	protected:
		void setView(sview view) {
			_view = view;
			source.resize(0);
		}

		sview getView() const {
			if (source.empty()) {
				return _view;
			}
			return sview { source.data(), source.size() };
		}
	};

	class OptionList;
	class OptionMap;
	class OptionSequence;
	struct OptionSeparated;
	// Class, that allows you to parse options.
	class Option : public AbstractOption<Option> {

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
		OptionSequence asSequence(wchar_t optionBegin = L'[', wchar_t optionEnd = L']', wchar_t paramDelimiter = L',', wchar_t optionDelimiter = L' ') const;

		bool empty() const;

	private:
		static double parseNumber(sview source);
	};

	struct OptionSeparated {
		Option first;
		Option rest;

		OptionSeparated() = default;
		OptionSeparated(Option first, Option rest)
			: first(std::move(first)),
			rest(std::move(rest)) { }
	};

	class OptionSequence : public AbstractOption<OptionSequence> {
		std::vector<std::vector<SubstringViewInfo>> list;

	public:
		OptionSequence() = default;
		OptionSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter);

		class iterator {
			sview view;
			const std::vector<std::vector<SubstringViewInfo>> &list;
			index ind;

		public:
			iterator(sview view, const std::vector<std::vector<SubstringViewInfo>> &list, index _index);

			iterator& operator++();

			bool operator !=(const iterator& other) const;

			OptionList operator*() const;
		};

		iterator begin() const;

		iterator end() const;

	private:
		static void emitToken(std::vector<SubstringViewInfo>& description, index begin, index end);
	};

	// List of string.
	class OptionList : public AbstractOption<OptionList> {
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
	};

	class OptionMap : public AbstractOption<OptionMap> {
	public:
		struct MapOptionInfo {
			SubstringViewInfo substringInfo { };
			bool touched = false;

			MapOptionInfo() = default;
			MapOptionInfo(SubstringViewInfo substringInfo) : substringInfo(substringInfo) { }
		};

	private:
		// For move and copy operations. 
		// String view would require much more hustle when moved with source than SubstringViewInfo
		std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };

		// For fast search.
		mutable std::map<isview, MapOptionInfo> params { };

	public:
		OptionMap() = default;
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

		std::vector<isview> getListOfUntouched() const;
	private:
		void fillParams() const;
		MapOptionInfo* find(isview name) const;
	};



	class OptionParser {
	public:
		static Option parse(sview string);
	};
}
