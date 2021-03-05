// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "case_insensitive_string.h"

std::wostream& rxtd::std_fixes::operator<<(std::wostream& stream, isview view) {
	return stream << (view % csView());
}
