/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "string_conversion.h"

rxtd::std_fixes::istring rxtd::std_fixes::string_conversion::operator%(string&& str, details::caseInsensitiveObject) {
	istring result{ reinterpret_cast<istring&&>(str) };
	return result;
}

rxtd::string rxtd::std_fixes::string_conversion::operator%(istring&& str, details::caseSensitiveObject) {
	string result{ reinterpret_cast<string&&>(str) };
	return result;
}

rxtd::sview rxtd::std_fixes::string_conversion::operator%(isview view, details::caseSensitiveViewObject) {
	return { view.data(), view.length() };
}

rxtd::string rxtd::std_fixes::string_conversion::operator%(sview view, details::stringCreationObject) {
	return { view.data(), view.length() };
}

rxtd::std_fixes::istring rxtd::std_fixes::string_conversion::operator%(isview view, details::stringCreationObject) {
	return { view.data(), view.length() };
}
