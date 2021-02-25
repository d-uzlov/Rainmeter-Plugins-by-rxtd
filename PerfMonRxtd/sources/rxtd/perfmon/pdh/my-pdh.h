/* 
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

// my-windows.h must be before Pdh.h
// because Pdh.h includes windows.h,
// which have some unwanted defines,
// that are disabled in my-windows.h
#include "my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Pdh.h>
