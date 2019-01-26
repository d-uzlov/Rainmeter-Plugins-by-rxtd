/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <string>
#include <vector>

namespace rxu {
	class SubstringViewInfo {
		size_t offset { };
		size_t length { };

	public:
		SubstringViewInfo();
		SubstringViewInfo(size_t offset, size_t length);

		bool empty() const;

		std::wstring_view makeView(const wchar_t* base) const;

		std::wstring_view makeView(std::wstring_view base) const;

		std::wstring_view makeView(const std::vector<wchar_t>& base) const;

		SubstringViewInfo substr(size_t subOffset, size_t subLength = std::wstring_view::npos) const;

		bool operator <(const SubstringViewInfo& other) const;
	};

	class StringViewUtils {
	public:
		static std::wstring_view trim(std::wstring_view view);

		static SubstringViewInfo trim(const wchar_t* base, SubstringViewInfo viewInfo);

		static SubstringViewInfo trim(std::wstring_view source, SubstringViewInfo viewInfo);
	};

	class StringUtils {
	public:
		static void lowerInplace(std::wstring& string);
		static void upperInplace(std::wstring& string);

		static std::wstring copyLower(std::wstring_view string);
		static std::wstring copyUpper(std::wstring_view string);

		static std::wstring trimCopy(std::wstring_view string);

		static void trimInplace(std::wstring &string);

		static bool endsWith(std::wstring_view str, std::wstring_view suffix);

		static bool startsWith(std::wstring_view str, std::wstring_view prefix);
	};
}
