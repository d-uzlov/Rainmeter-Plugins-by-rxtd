// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
	return string{ view };
}

rxtd::istring rxtd::std_fixes::string_conversion::operator%(isview view, details::stringCreationObject) {
	return istring{ view };
}
