// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include "FftSizeHelper.h"

using rxtd::fft_utils::FftSizeHelper;

rxtd::index FftSizeHelper::isAllowedSize(index val) {
	while ((val % 2) == 0) val /= 2;
	while ((val % 3) == 0) val /= 3;
	while ((val % 5) == 0) val /= 5;
	return val <= 1;
}

rxtd::index FftSizeHelper::findNextNativeAllowedLength(index l) {
	while (!isAllowedSize(l)) l++;
	return l;
}

rxtd::index FftSizeHelper::findNextAllowedLength(index l, bool real) {
	if (l == 0) {
		return 0;
	}
	if (real) {
		l += 1;
		l /= 2;
	}
	constexpr index pffftMinFftSize = 16;
	l += pffftMinFftSize - 1;
	l /= pffftMinFftSize;
	l = findNextNativeAllowedLength(l);
	l *= pffftMinFftSize;
	if (real) {
		l *= 2;
	}
	return l;
}
