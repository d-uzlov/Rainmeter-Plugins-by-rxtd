/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionList.h"
#include "Tokenizer.h"

#include "undef.h"

using namespace utils;

OptionList::OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
	AbstractOption(view),
	list(std::move(list)) {
}

std::pair<sview, std::vector<SubstringViewInfo>> OptionList::consume() && {
	return { getView(), std::move(list) };
}

index OptionList::size() const {
	return list.size();
}

bool OptionList::empty() const {
	return list.empty();
}

Option OptionList::get(index ind) const {
	return Option { list[ind].makeView(getView()) };
}

OptionList::iterator::iterator(const OptionList& container, index _index):
	container(container),
	ind(_index) {
}

OptionList::iterator& OptionList::iterator::operator++() {
	ind++;
	return *this;
}

bool OptionList::iterator::operator!=(const iterator& other) const {
	return &container != &other.container || ind != other.ind;
}

Option OptionList::iterator::operator*() const {
	return container.get(ind);
}

OptionList::iterator OptionList::begin() const {
	return { *this, 0 };
}

OptionList::iterator OptionList::end() const {
	return { *this, size() };
}
