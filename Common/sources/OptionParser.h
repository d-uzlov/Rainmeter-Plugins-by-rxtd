/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "Color.h"
#include "StringUtils.h"
#include <optional>

namespace rxu {
	class OptionParser {
		typedef StringViewUtils svu;

		class Tokenizer {
			std::vector<SubstringViewInfo> tempList { };

		public:
			std::vector<SubstringViewInfo> parse(std::wstring_view string, wchar_t delimiter);

		private:
			void emitToken(const size_t begin, const size_t end);

			void tokenize(std::wstring_view string, wchar_t delimiter);

			std::vector<SubstringViewInfo> trimSpaces(std::wstring_view string);
		};

		class Option {
			std::wstring_view view;

		public:
			Option();
			explicit Option(std::wstring_view view);

			std::wstring_view asString(std::wstring_view defaultValue = { }) const;

			double asFloat(double defaultValue = 0.0) const;

			int64_t asInt(int64_t defaultValue = 0) const;

			bool asBool(bool defaultValue = false) const;

			Color asColor(Color defaultValue = { }) const;

		private:
			static double parseNumber(std::wstring_view source);
		};

		Tokenizer tokenizer;
	public:
		class OptionList {
			std::vector<wchar_t> source;
			std::vector<SubstringViewInfo> list;

		public:
			OptionList();

			OptionList(std::wstring_view string, std::vector<SubstringViewInfo>&& list);

			std::pair<std::vector<wchar_t>, std::vector<SubstringViewInfo>> consume() &&;

			size_t size() const;
			bool empty() const;

			std::wstring_view get(size_t index) const;
			Option getOption(size_t index) const;

			class iterator {
				OptionList &container;
				std::ptrdiff_t index;

			public:
				iterator(OptionList& container, std::ptrdiff_t index) :
					container(container),
					index(index) { }

				iterator& operator++() {
					index++;
					return *this;
				}
				bool operator !=(const iterator& other) const {
					return &container != &other.container || index != other.index;
				}
				std::wstring_view operator*() const {
					return container.get(index);
				}
			};

			iterator begin() {
				return { *this, 0 };
			}
			iterator end() {
				return { *this, static_cast<std::ptrdiff_t>(size()) };
			}

			class const_iterator {
				const OptionList &container;
				std::ptrdiff_t index;

			public:
				const_iterator(const OptionList& container, std::ptrdiff_t index) :
					container(container),
					index(index) { }

				const_iterator& operator++() {
					index++;
					return *this;
				}
				bool operator !=(const const_iterator& other) const {
					return &container != &other.container || index != other.index;
				}
				std::wstring_view operator*() const {
					return container.get(index);
				}
			};

			const_iterator begin() const {
				return { *this, 0 };
			}
			const_iterator end() const {
				return { *this, static_cast<std::ptrdiff_t>(size()) };
			}
		};

		class OptionMap {
			std::wstring stringLower;
			std::wstring stringOriginal;
			std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };

			std::map<std::wstring_view, SubstringViewInfo> params { };

			mutable std::wstring nameBuffer;

		public:
			OptionMap();
			OptionMap(std::wstring&& string, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo);
			~OptionMap();

			OptionMap(const OptionMap& other);
			OptionMap(OptionMap&& other) noexcept;
			OptionMap& operator=(const OptionMap& other);
			OptionMap& operator=(OptionMap&& other) noexcept;

			Option get(std::wstring_view name) const;
			Option getCS(std::wstring_view name) const;

			const std::map<std::wstring_view, SubstringViewInfo>& getParams() const;
		private:
			void fillParams();
			std::optional<SubstringViewInfo> find(std::wstring_view name) const;
		};

		OptionList asList(std::wstring_view string, wchar_t delimiter);
		OptionMap asMap(std::wstring string, wchar_t optionDelimiter, wchar_t nameDelimiter);
		OptionMap asMap(std::wstring_view string, wchar_t optionDelimiter, wchar_t nameDelimiter );
		OptionMap asMap(const wchar_t *string, wchar_t optionDelimiter, wchar_t nameDelimiter);
	};
}
