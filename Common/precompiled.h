/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <string_view>
#include <cwctype>
#include <utility>
#include <optional>

#include "array_view.h"
#include "typedefs.h"
#include "CaseInsensitiveString.h"
#include "GenericBaseClasses.h"

template<typename T>
std::optional<T> parseEnum(isview) {
	static_assert(false);
}

#ifndef NOMINMAX
#define NOMINMAX
#endif
