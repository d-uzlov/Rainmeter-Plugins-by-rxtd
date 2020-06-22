/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "StringUtils.h"

#include "undef.h"

using namespace utils;

SubstringViewInfo::SubstringViewInfo(index offset, index length) :
	offset(offset),
	length(length) { }

bool SubstringViewInfo::empty() const {
	return length == 0;
}

sview SubstringViewInfo::makeView(const wchar_t* base) const {
	return sview(base + offset, length);
}

sview SubstringViewInfo::makeView(const std::vector<wchar_t>& base) const {
	return makeView(sview { base.data(), base.size() });
}

SubstringViewInfo SubstringViewInfo::substr(index subOffset,
	index subLength) const {
	if (subOffset >= length) {
		return { };
	}
	const auto newOffset = offset + subOffset;
	const auto newLength = std::min(length - subOffset, subLength);
	return { newOffset, newLength };
}

bool SubstringViewInfo::operator<(const SubstringViewInfo& other) const {
	if (offset < other.offset) {
		return true;
	}
	if (offset > other.offset) {
		return false;
	}

	return length < other.length;
}



SubstringViewInfo StringUtils::trimInfo(const wchar_t* base, SubstringViewInfo viewInfo) {
	sview view = viewInfo.makeView(base);

	const auto begin = view.find_first_not_of(L" \t");
	if (begin == sview::npos) {
		return { };
	}

	const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

	return viewInfo.substr(begin, end - begin + 1);
}

SubstringViewInfo StringUtils::trimInfo(sview source, SubstringViewInfo viewInfo) {
	return trimInfo(source.data(), viewInfo);
}
