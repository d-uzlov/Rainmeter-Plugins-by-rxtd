/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MyWaveFormat.h"

#include "undef.h"

rxaa::Format::Format(Value value): value(value) {
}

bool rxaa::Format::operator==(Format a) const {
	return value == a.value;
}

bool rxaa::Format::operator!=(Format a) const {
	return value != a.value;
}

const wchar_t* rxaa::Format::toString() const {
	if (value == INVALID) {
		return L"<invalid>";
	}
	if (value == PCM_S16) {
		return L"PCM 16b";
	}
	if (value == PCM_F32) {
		return L"PCM 32b";
	}
	return nullptr;
}
