/* Copyright (C) 2004 Rainmeter Project Developers
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <string>
#include <cwctype>
#include <vector>
#include <algorithm>

inline bool indexIsInBounds(int index, int min, int max) {
	return index >= min && index <= max;
}
inline std::wstring trimSpaces(const std::wstring &string) {
	const auto wsfront = std::find_if_not(string.begin(), string.end(), [](int c) {return std::iswspace(c); });
	return std::wstring(
		wsfront, 
		std::find_if_not(string.rbegin(), std::wstring::const_reverse_iterator(wsfront), [](int c) {return std::iswspace(c); }).base()
	);
}

inline std::vector<std::wstring> Tokenize(const std::wstring& str, const std::wstring& delimiters) {
	std::vector<std::wstring> tokens;

	// Copied from Rainmeter library

	size_t pos = 0;
	do {
		size_t lastPos = str.find_first_not_of(delimiters, pos);
		if (lastPos == std::wstring::npos) break;

		pos = str.find_first_of(delimiters, lastPos + 1);
		std::wstring token = str.substr(lastPos, pos - lastPos);  // len = (pos != std::wstring::npos) ? pos - lastPos : pos

		size_t pos2 = token.find_first_not_of(L" \t\r\n");
		if (pos2 != std::wstring::npos) {
			size_t lastPos2 = token.find_last_not_of(L" \t\r\n");
			if (pos2 != 0 || lastPos2 != (token.size() - 1)) {
				// Trim white-space
				token.assign(token, pos2, lastPos2 - pos2 + 1);
			}
			tokens.push_back(token);
		}

		if (pos == std::wstring::npos) break;
		++pos;
	} while (true);

	return tokens;
}

inline bool stringsMatch(const wchar_t* str1, const wchar_t* str2, bool matchPartial) {
	if (matchPartial) {
		// check if substring str2 occurs in str1
		return wcsstr(str1, str2) != nullptr;
	}
	// check if str1 and str2 match exactly
	return wcscmp(str1, str2) == 0;
}
