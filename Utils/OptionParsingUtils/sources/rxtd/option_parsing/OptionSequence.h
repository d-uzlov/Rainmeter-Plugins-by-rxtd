// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "OptionBase.h"
#include "OptionList.h"
#include "Tokenizer.h"
#include "rxtd/std_fixes/StringUtils.h"

namespace rxtd::option_parsing {
	//
	// Parses its contents in the form:
	// <name> <optionBegin> <value> <optionDelimiter> <value> <optionDelimiter> ... <optionEnd> ...
	//
	class OptionSequence : public OptionBase {
		using SubstringViewInfo = std_fixes::SubstringViewInfo;

		std::vector<std::vector<SubstringViewInfo>> list;

	public:
		OptionSequence() = default;

		OptionSequence(
			sview view,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter
		) : OptionBase(view) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter);
		}

		OptionSequence(
			SourceType&& source,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter
		) : OptionBase(std::move(source)) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter);
		}

		class iterator {
			sview view;
			array_view<std::vector<SubstringViewInfo>> list;
			index ind;

		public:
			iterator(sview view, array_view<std::vector<SubstringViewInfo>> list, index _index) :
				view(view), list(list), ind(_index) { }

			iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const iterator& other) const {
				return list.data() != other.list.data() || ind != other.ind;
			}

			[[nodiscard]]
			OptionList operator*() const {
				auto list_ = list[ind];
				return { view, std::move(list_) };
			}
		};

		[[nodiscard]]
		iterator begin() const {
			return { getView(), list, 0 };
		}

		[[nodiscard]]
		iterator end() const {
			return { getView(), list, static_cast<index>(list.size()) };
		}
	};
}
