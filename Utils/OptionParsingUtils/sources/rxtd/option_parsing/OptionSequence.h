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

		std::vector<std::pair<SubstringViewInfo, SubstringViewInfo>> list;

	public:
		OptionSequence() = default;

		OptionSequence(
			sview view,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter,
			const Logger& cl
		) : OptionBase(view) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter, cl);
		}

		OptionSequence(
			SourceType&& source,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter,
			const Logger& cl
		) : OptionBase(std::move(source)) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter, cl);
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
			std::pair<GhostOption, GhostOption> operator*() const {
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
		std::pair<GhostOption, GhostOption> getElement(index i) const {
			auto pair = list[static_cast<size_t>(i)];
			return { GhostOption{ pair.first.makeView(getView()) }, GhostOption{ pair.second.makeView(getView()) } };
		}
	};
}
