// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "StringUtils.h"

#include "rxtd/my-windows.h"

using rxtd::std_fixes::SubstringViewInfo;
using rxtd::std_fixes::StringUtils;

SubstringViewInfo SubstringViewInfo::substr(
	index subOffset,
	index subLength
) const {
	if (subOffset >= length) {
		return {};
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

void StringUtils::makeUppercaseInPlace(sview str) {
	// waiting for std::string_span to arrive to c++ standard.
	// Hopefully this will require no more than 10-15 years.
	auto* data = const_cast<wchar_t*>(str.data());

	CharUpperBuffW(data, static_cast<DWORD>(str.length()));
}


SubstringViewInfo StringUtils::trimInfo(sview source, SubstringViewInfo viewInfo) {
	sview view = viewInfo.makeView(source);

	const auto begin = view.find_first_not_of(L" \t");
	if (begin == sview::npos) {
		return {};
	}

	const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

	return viewInfo.substr(static_cast<index>(begin), static_cast<index>(end - begin + 1));
}

rxtd::index StringUtils::parseInt(sview view, bool forceHex) {
	view = trim(view);

	if (checkStartsWith(view, L"0x") || checkStartsWith(view, L"0X")) {
		forceHex = true;
		view = view.substr(2);
	} else if (view[0] == L'+') {
		view = view.substr(1);
	}

	if (!forceHex && view.length() > 0 && !iswdigit(view.front())
		|| forceHex && view.length() > 0 && !iswxdigit(view.front())) {
		return 0;
	}

	char buffer[80];
	index size = std::min(static_cast<index>(view.length()), static_cast<index>(sizeof(buffer) / sizeof(*buffer)));
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
	const int base = forceHex ? 16 : 10;
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
	index size = std::min(static_cast<index>(view.length()), static_cast<index>(sizeof(buffer) / sizeof(*buffer)));
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
