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

index StringUtils::parseInt(sview view) {
	view = trim(view);

	bool hex = false;
	if (checkStartsWith(view, L"0x"sv) || checkStartsWith(view, L"0X"sv)) {
		hex = true;
		view = view.substr(2);
	} else if (view[0] == L'+') {
		view = view.substr(1);
	}

	if (view.length() > 0 && !iswdigit(view.front())) {
		return 0;
	}

	char buffer[80];
	index size = std::min(view.length(), sizeof(buffer) / sizeof(*buffer));
	for (index i = 0; i < size; ++i) {
		const auto wc = view[i];
		// check that all symbols fit into 1 byte
		if (wc > std::numeric_limits<uint8_t>::max()) {
			buffer[i] = '\0';
			size = i;
			break;
		}
		buffer[i] = static_cast<char>(wc);
	}

	index result = 0;
	const int base = hex ? 16 : 10;
	std::from_chars(buffer, buffer + size, result, base);
	return result;
}

double StringUtils::parseFloat(sview view) {
	view = trim(view);

	if (view[0] == L'+') {
		view = view.substr(1);
	}

	if (view.empty() || !iswdigit(view.front())) {
		return 0;
	}

	char buffer[80];
	index size = std::min(view.length(), sizeof(buffer) / sizeof(*buffer));
	for (index i = 0; i < size; ++i) {
		const auto wc = view[i];
		// check that all symbols fit into 1 byte
		if (wc > std::numeric_limits<uint8_t>::max()) {
			buffer[i] = '\0';
			size = i;
			break;
		}
		buffer[i] = static_cast<char>(wc);
	}

	double result = 0;
	std::from_chars(buffer, buffer + size, result);
	return result;
}

double StringUtils::parseFractional(sview view) {
	view = trim(view);

	if (view.empty() || !iswdigit(view.front())) {
		return 0;
	}

	// TODO remove this mess
	char buffer[80];
	buffer[0] = '0';
	buffer[1] = '.';

	index size = std::min(view.length(), sizeof(buffer) / sizeof(*buffer));
	for (index i = 0; i < size; ++i) {
		const auto wc = view[i];
		// check that all symbols fit into 1 byte
		if (wc > std::numeric_limits<uint8_t>::max()) {
			buffer[i + 2] = '\0';
			size = i;
			break;
		}
		buffer[i + 2] = static_cast<char>(wc);
	}

	double result = 0;
	std::from_chars(buffer, buffer + size + 2, result);
	return result;
}
