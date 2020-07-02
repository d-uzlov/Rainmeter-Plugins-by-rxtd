/*
 * Copyright (C) 2019 rxtd
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
}
