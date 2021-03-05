// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

// for some reason disabling C4599 in settings doesn't work
#pragma warning(disable : 4599)

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

#include "rxtd/GenericBaseClasses.h"
#include "rxtd/std_fixes/array_view.h"

namespace rxtd {
	using index = ptrdiff_t;
}

#include "rxtd/std_fixes/StringBaseExtended.h"

namespace rxtd {
	template<class Elem, class Traits = std::char_traits<Elem>, class Alloc = std::allocator<Elem>>
	using StringBase = std_fixes::StringBaseExtended<Elem, Traits, Alloc>;
	template<class Elem, class Traits = std::char_traits<Elem>, class Alloc = std::allocator<Elem>>
	using StringViewBase = std_fixes::StringViewBaseExtended<Elem, Traits>;
	
	using string = StringBase<wchar_t>;
	using sview = StringViewBase<wchar_t>;
}

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
