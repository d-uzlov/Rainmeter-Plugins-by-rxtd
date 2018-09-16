/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <pdh.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cwctype>


namespace pmr {
	struct modifiedNameItem {
		const wchar_t* originalName;
		const wchar_t* uniqueName;
		const wchar_t* displayName;
		const wchar_t* searchName;
	};

	struct indexesItem {
		unsigned long originalCurrentInx;
		unsigned long originalPreviousInx;
	};

	struct instanceKeyItem {
		const wchar_t* sortName;
		double sortValue;
		indexesItem originalIndexes;
		std::vector<indexesItem> vectorIndexes;
	};

	class ParentData;
	class ChildData;

	struct TypeHolder {
		bool isParent = false;
		union {
			ParentData* parentData = nullptr;
			ChildData* childData;
		};

		// common data

		void* rm = nullptr;
		void* skin = nullptr;
		std::wstring measureName;
		bool broken = false;
		bool temporaryBroken = false;

		double resultDouble = 0.0;
		const wchar_t* resultString = nullptr;

		TypeHolder() { }
		~TypeHolder();
		bool isBroken() const {
			return broken || temporaryBroken;
		}
	};
	
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

	bool stringsMatch(LPCWSTR str1, LPCWSTR str2, bool matchPartial);
	std::vector<std::wstring> Tokenize(const std::wstring& str, const std::wstring& delimiters);

	extern std::vector<ParentData*> parentMeasuresVector;
}
