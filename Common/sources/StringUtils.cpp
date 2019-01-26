/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "StringUtils.h"
#include <algorithm>
#include <cwctype>

rxu::SubstringViewInfo::SubstringViewInfo() = default;

rxu::SubstringViewInfo::SubstringViewInfo(size_t offset, size_t length) :
	offset(offset),
	length(length) { }

bool rxu::SubstringViewInfo::empty() const {
	return length == 0;
}

std::wstring_view rxu::SubstringViewInfo::makeView(const wchar_t* base) const {
	return { base + offset, length };
}

std::wstring_view rxu::SubstringViewInfo::makeView(std::wstring_view base) const {
	return { base.data() + offset, length };
}

std::wstring_view rxu::SubstringViewInfo::makeView(const std::vector<wchar_t>& base) const {
	return makeView(std::wstring_view { base.data(), base.size() });
}

rxu::SubstringViewInfo rxu::SubstringViewInfo::substr(size_t subOffset,
	size_t subLength) const {
	if (subOffset >= length) {
		return { };
	}
	const auto newOffset = offset + subOffset;
	const auto newLength = std::min(length - subOffset, subLength);
	return { newOffset, newLength };
}

bool rxu::SubstringViewInfo::operator<(const SubstringViewInfo& other) const {
	if (offset < other.offset) {
		return true;
	}
	if (offset > other.offset) {
		return false;
	}

	return length < other.length;
}



std::wstring_view rxu::StringViewUtils::trim(std::wstring_view view) {
	const auto begin = view.find_first_not_of(L" \t");
	if (begin == std::remove_reference<decltype(view)>::type::npos) {
		return { };
	}

	const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

	return { view.data() + begin, end - begin + 1 };
}

rxu::SubstringViewInfo rxu::StringViewUtils::trim(const wchar_t* base, SubstringViewInfo viewInfo) {
	std::wstring_view view = viewInfo.makeView(base);

	const auto begin = view.find_first_not_of(L" \t");
	if (begin == std::remove_reference<decltype(view)>::type::npos) {
		return { };
	}

	const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

	return viewInfo.substr(begin, end - begin + 1);
}

rxu::SubstringViewInfo rxu::StringViewUtils::trim(std::wstring_view source, SubstringViewInfo viewInfo) {
	return trim(source.data(), viewInfo);
}

void rxu::StringUtils::lowerInplace(std::wstring& string) {
	for (auto& c : string) {
		c = std::towlower(c);
	}
}

void rxu::StringUtils::upperInplace(std::wstring& string) {
	for (auto& c : string) {
		c = std::towupper(c);
	}
}

std::wstring rxu::StringUtils::copyLower(std::wstring_view string) {
	std::wstring buffer { string };
	lowerInplace(buffer);

	return buffer;
}

std::wstring rxu::StringUtils::copyUpper(std::wstring_view string) {
	std::wstring buffer { string };
	upperInplace(buffer);

	return buffer;
}

std::wstring rxu::StringUtils::trimCopy(std::wstring_view string) {
	// from stackoverflow
	const auto firstNotSpace = std::find_if_not(string.begin(), string.end(), [](wint_t c) { return std::iswspace(c); });
	const auto lastNotSpace = std::find_if_not(string.rbegin(), std::wstring_view::const_reverse_iterator(firstNotSpace), [](wint_t c) { return std::iswspace(c); }).base();

	return { firstNotSpace, lastNotSpace };
}

void rxu::StringUtils::trimInplace(std::wstring& string) {
	const auto firstNotSpace = std::find_if_not(string.begin(), string.end(), [](wint_t c) { return std::iswspace(c); });
	string.erase(string.begin(), firstNotSpace);
	const auto lastNotSpace = std::find_if_not(string.rbegin(), string.rend(), [](wint_t c) { return std::iswspace(c); }).base();
	string.erase(lastNotSpace, string.end());
}

bool rxu::StringUtils::endsWith(std::wstring_view str, std::wstring_view suffix) {
	// from stackoverflow
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool rxu::StringUtils::startsWith(std::wstring_view str, std::wstring_view prefix) {
	// from stackoverflow
	return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

