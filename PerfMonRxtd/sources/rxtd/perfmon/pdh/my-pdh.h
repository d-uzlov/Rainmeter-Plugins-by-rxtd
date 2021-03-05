// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

// my-windows.h must be before Pdh.h
// because Pdh.h includes windows.h,
// which have some unwanted defines,
// that are disabled in my-windows.h
#include "rxtd/my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Pdh.h>
