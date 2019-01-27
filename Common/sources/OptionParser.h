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

namespace rxtd::utils {
	class OptionParser {
		typedef StringUtils svu;

		class Tokenizer {
			std::vector<SubstringViewInfo> tempList { };

		public:
			std::vector<SubstringViewInfo> parse(sview string, wchar_t delimiter);

		private:
			void emitToken(index begin, index end);

			void tokenize(sview string, wchar_t delimiter);

			std::vector<SubstringViewInfo> trimSpaces(sview string);
		};

		// Class, that allows you to parse options.
		class Option {
			sview view;

		public:
			Option();
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

		private:
			static double parseNumber(sview source);
		};

		Tokenizer tokenizer;
	public:
		// List of string.
		class OptionList {
			std::vector<wchar_t> source;
			std::vector<SubstringViewInfo> list;

		public:
			OptionList();

			OptionList(sview view, std::vector<SubstringViewInfo>&& list);

			// Allows you to steal inner resources.
			std::pair<std::vector<wchar_t>, std::vector<SubstringViewInfo>> consume() && ;

			// Count of elements in list.
			index size() const;
			// Alias to "size() == 0".
			bool empty() const;

			// View of Nth option.
			sview get(index ind) const;
			// Case-insensitive view of Nth option.
			isview getCI(index ind) const;
			// Parseable view of Nth option.
			Option getOption(index ind) const;

			class iterator {
				const OptionList& container;
				index ind;

			public:
				iterator(const OptionList& container, index _index);

				iterator& operator++();

				bool operator !=(const iterator& other) const;

				sview operator*() const;
			};

			iterator begin() const;

			iterator end() const;

			class CaseInsensitiveView {
				const OptionList& container;

			public:
				explicit CaseInsensitiveView(const OptionList& basicStringViews);

				class iterator {
					const OptionList& container;
					index ind;

				public:
					iterator(const OptionList& container, index _index);

					iterator& operator++();

					bool operator !=(const iterator& other) const;

					isview operator*() const;
				};

				iterator begin() const;

				iterator end() const;
			};

			// Allows you to iterate over case-insensitive views.
			CaseInsensitiveView viewCI() const;
		};

		class OptionMap {
			// All string views targets here
			string source;
			// For move and copy operations.
			std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };

			// For fast search.
			std::map<isview, SubstringViewInfo> params { };

		public:
			OptionMap();
			OptionMap(string&& string, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo);
			~OptionMap();

			OptionMap(const OptionMap& other);
			OptionMap(OptionMap&& other) noexcept;
			OptionMap& operator=(const OptionMap& other);
			OptionMap& operator=(OptionMap&& other) noexcept;

			//Returns named option, search is case-insensitive.
			Option get(sview name) const;

			//Returns named option, search is case-insensitive.
			Option get(isview name) const;
			
			//Returns named option, search is case-insensitive.
			Option get(const wchar_t* name) const;

			// Allows you to iterate over all available options.
			const std::map<isview, SubstringViewInfo>& getParams() const;
		private:
			void fillParams();
			std::optional<SubstringViewInfo> find(isview name) const;
		};
		// Parses and creates OptionList.
		OptionList asList(sview string, wchar_t delimiter);
		// Parses and creates OptionList.
		OptionList asList(isview string, wchar_t delimiter);
		// Parses and creates OptionList.
		OptionList asList(const wchar_t *string, wchar_t delimiter);

		// Parses and creates OptionMap.
		OptionMap asMap(string&& string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		// Parses and creates OptionMap.
		OptionMap asMap(istring&& string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		// Parses and creates OptionMap.
		OptionMap asMap(sview string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		// Parses and creates OptionMap.
		OptionMap asMap(isview string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		// Parses and creates OptionMap.
		OptionMap asMap(const wchar_t *string, wchar_t optionDelimiter, wchar_t nameDelimiter);
	};
}
