/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "StringUtils.h"
#include "AbstractOption.h"
#include "OptionList.h"
#include "Tokenizer.h"

namespace rxtd::utils {
	class OptionSequence : public AbstractOption<OptionSequence> {
		std::vector<std::vector<SubstringViewInfo>> list;

	public:
		OptionSequence() = default;

		OptionSequence(
			sview view, std::vector<wchar_t>&& source,
			wchar_t optionBegin, wchar_t optionEnd,
			wchar_t optionDelimiter
		) : AbstractOption(view, std::move(source)) {
			list = Tokenizer::parseSequence(getView(), optionBegin, optionEnd, optionDelimiter);
		}

		class iterator {
			sview view;
			const std::vector<std::vector<SubstringViewInfo>>& list;
			index ind;

		public:
			iterator(sview view, const std::vector<std::vector<SubstringViewInfo>>& list, index _index) :
				view(view), list(list), ind(_index) {
			}

			iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const iterator& other) const {
				return &list != &other.list || ind != other.ind;
			}

			[[nodiscard]]
			OptionList operator*() const {
				auto list_ = list[ind];
				return { view, { }, std::move(list_) };
			}
		};

		[[nodiscard]]
		iterator begin() const {
			return { getView(), list, 0 };
		}

		[[nodiscard]]
		iterator end() const {
			return { getView(), list, index(list.size()) };
		}
	};
}
