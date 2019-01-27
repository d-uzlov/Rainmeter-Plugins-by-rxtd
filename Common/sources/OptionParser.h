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
		typedef StringViewUtils svu;

		class Tokenizer {
			std::vector<SubstringViewInfo> tempList { };

		public:
			std::vector<SubstringViewInfo> parse(sview string, wchar_t delimiter);

		private:
			void emitToken(const index begin, const index end);

			void tokenize(sview string, wchar_t delimiter);

			std::vector<SubstringViewInfo> trimSpaces(sview string);
		};

		class Option {
			sview view;

		public:
			Option();
			explicit Option(sview view);

			sview asString(sview defaultValue = { }) const;

			double asFloat(double defaultValue = 0.0) const;

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

			bool asBool(bool defaultValue = false) const {
				return asFloat(defaultValue ? 1.0 : 0.0) != 0.0;
			}

			Color asColor(Color defaultValue = { }) const;

		private:
			static double parseNumber(sview source);
		};

		Tokenizer tokenizer;
	public:
		class OptionList {
			std::vector<wchar_t> source;
			std::vector<SubstringViewInfo> list;

		public:
			OptionList();

			OptionList(sview view, std::vector<SubstringViewInfo>&& list);

			std::pair<std::vector<wchar_t>, std::vector<SubstringViewInfo>> consume() && ;

			index size() const;
			bool empty() const;

			sview get(index index) const;
			Option getOption(index index) const;

			template<typename C>
			class iterator {
				C &container;
				index ind;

			public:
				iterator(C& container, index _index) :
					container(container),
					ind(_index) { }

				iterator& operator++() {
					ind++;
					return *this;
				}
				bool operator !=(const iterator& other) const {
					return &container != &other.container || ind != other.ind;
				}
				sview operator*() const {
					return container.get(ind);
				}
			};

			iterator<const OptionList> begin() const {
				return { *this, 0 };
			}
			iterator<const OptionList> end() const {
				return { *this, size() };
			}
		};

		class OptionMap {
			string source;
			std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };

			std::map<isview, SubstringViewInfo> params { };

		public:
			OptionMap();
			OptionMap(string&& string, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo);
			~OptionMap();

			OptionMap(const OptionMap& other);
			OptionMap(OptionMap&& other) noexcept;
			OptionMap& operator=(const OptionMap& other);
			OptionMap& operator=(OptionMap&& other) noexcept;

			Option get(sview name) const;
			Option get(isview name) const;
			Option get(const wchar_t* name) const;

			const std::map<isview, SubstringViewInfo>& getParams() const;
		private:
			void fillParams();
			std::optional<SubstringViewInfo> find(isview name) const;
		};

		OptionList asList(sview string, wchar_t delimiter);
		OptionMap asMap(string string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		OptionMap asMap(sview string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		OptionMap asMap(const wchar_t *string, wchar_t optionDelimiter, wchar_t nameDelimiter);
	};
}
