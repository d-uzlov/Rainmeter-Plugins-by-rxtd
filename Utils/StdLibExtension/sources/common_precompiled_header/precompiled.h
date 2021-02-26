/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <algorithm>
#include <any>
#include <cstdint>
#include <cwctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rxtd {
	using index = ptrdiff_t;
	using string = std::wstring;
	using sview = std::wstring_view;
}

#include "rxtd/GenericBaseClasses.h"
#include "rxtd/std_fixes/array_view.h"
#include "rxtd/std_fixes/case_insensitive_string.h"

namespace rxtd {
	using istring = std_fixes::istring;
	using isview = std_fixes::isview;

	using std_fixes::operator<<;
}

#include "rxtd/std_fixes/string_conversion.h"

namespace rxtd {
	using namespace std_fixes::string_conversion;
}

template<typename T>
std::optional<T> parseEnum(rxtd::isview) {
	// ReSharper disable once CppStaticAssertFailure
	static_assert(false, "Template specialization of parseEnum() must be created by user.");
	return {};
}
