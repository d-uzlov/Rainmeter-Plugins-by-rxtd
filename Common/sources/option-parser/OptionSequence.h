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
#include "Option.h"

namespace rxtd::utils {
	class OptionSequence : public AbstractOption<OptionSequence> {
		std::vector<std::vector<SubstringViewInfo>> list;

	public:
		OptionSequence() = default;
		OptionSequence(sview view, std::vector<wchar_t> &&source, wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter);

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
}
