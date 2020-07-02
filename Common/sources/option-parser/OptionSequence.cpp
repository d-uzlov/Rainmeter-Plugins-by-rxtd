/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionSequence.h"
#include "Tokenizer.h"

#include "OptionList.h"

#include "undef.h"

using namespace utils;

OptionSequence::OptionSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) : AbstractOption(view) {
	list = Tokenizer::parseSequence(view, optionBegin, optionEnd, paramDelimiter, optionDelimiter);
}

OptionSequence::iterator::iterator(sview view, const std::vector<std::vector<SubstringViewInfo>>& list, index _index) : 
	view(view), list(list), ind(_index) {
}

OptionSequence::iterator& OptionSequence::iterator::operator++() {
	ind++;
	return *this;
}

bool OptionSequence::iterator::operator!=(const iterator& other) const {
	return &list != &other.list || ind != other.ind;
}

OptionList OptionSequence::iterator::operator*() const {
	auto list_ = list[ind];
	return { view, std::move(list_) };
}

OptionSequence::iterator OptionSequence::begin() const {
	return { getView(), list, 0 };
}

OptionSequence::iterator OptionSequence::end() const {
	return { getView(), list, index(list.size()) };
}

void OptionSequence::emitToken(std::vector<SubstringViewInfo>& description, index begin, index end) {
	if (end <= begin) {
		return;
	}
	description.emplace_back(begin, end - begin);
}
