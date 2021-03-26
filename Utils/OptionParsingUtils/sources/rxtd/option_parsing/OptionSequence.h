// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "OptionBase.h"
#include "Tokenizer.h"
#include "rxtd/std_fixes/StringUtils.h"

namespace rxtd::option_parsing {
	//
	// Parses its contents in the form:
	// <name> <optionBegin> <value> <optionDelimiter> <value> <optionDelimiter> ... <optionEnd> ...
	//
	class OptionSequence : public OptionBase {
		using SubstringViewInfo = std_fixes::SubstringViewInfo;

	public:
		struct Element {
			GhostOption name;
			GhostOption args;
			GhostOption postfix;
		};

		std::vector<std::tuple<SubstringViewInfo, SubstringViewInfo, SubstringViewInfo>> list;

	public:
		OptionSequence() = default;

		OptionSequence(
			sview view,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter,
			bool allowPostfix,
			const Logger& cl
		) : OptionBase(view) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter, allowPostfix, cl);
		}

		OptionSequence(
			SourceType&& source,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter,
			bool allowPostfix,
			const Logger& cl
		) : OptionBase(std::move(source)) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter, allowPostfix, cl);
		}

		class iterator {
			const OptionSequence& parent;
			index ind;

		public:
			iterator(const OptionSequence& parent, index _index) :
				parent(parent), ind(_index) { }

			iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const iterator& other) const {
				return &parent != &other.parent || ind != other.ind;
			}

			[[nodiscard]]
			Element operator*() const {
				return parent.getElement(ind);
			}
		};

		[[nodiscard]]
		iterator begin() const {
			return { *this, 0 };
		}

		[[nodiscard]]
		iterator end() const {
			return { *this, getSize() };
		}

		[[nodiscard]]
		bool isEmpty() const {
			return getSize() == 0;
		}

		[[nodiscard]]
		index getSize() const {
			return static_cast<index>(list.size());
		}

		[[nodiscard]]
		Element getElement(index i) const {
			auto pair = list[static_cast<size_t>(i)];
			return {
				GhostOption{ std::get<0>(pair).makeView(getView()) },
				GhostOption{ std::get<1>(pair).makeView(getView()) },
				GhostOption{ std::get<2>(pair).makeView(getView()) },
			};
		}
	};
}
