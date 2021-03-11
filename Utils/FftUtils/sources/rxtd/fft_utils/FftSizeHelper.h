// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

namespace rxtd::fft_utils {
	// inspired by function kiss_fft_next_fast_size from kiss fft project
	// https://github.com/mborgerding/kissfft/blob/master/kiss_fft.c#L408
	class FftSizeHelper {
	public:
		static index findNextAllowedLength(index l, bool real);

	private:
		static index isAllowedSize(index val);
		static index findNextNativeAllowedLength(index l);
	};
}
