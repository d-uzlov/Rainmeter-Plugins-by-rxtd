/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::buffer_printer {
	//
	// This is the most basic implementation of std::basic_streambuf
	// Intended use is to allow read of std::wostream buffer without copying it
	//
	class ReadableStreamBuffer : public std::basic_streambuf<wchar_t> {
		std::vector<char_type> buffer;

	public:
		ReadableStreamBuffer() = default;
		~ReadableStreamBuffer() = default;

		// these constructors must be implemented explicitly because std::basic_streambuf is not movable,
		// so no default functions can be generated
		ReadableStreamBuffer(ReadableStreamBuffer&& other) noexcept;
		ReadableStreamBuffer& operator=(ReadableStreamBuffer&& other) noexcept;

		ReadableStreamBuffer(const ReadableStreamBuffer& other) = default;
		ReadableStreamBuffer& operator=(const ReadableStreamBuffer& other) = default;

		sview getBuffer();

		void resetPointers();

		void appendEOL();

	protected:
		int_type overflow(int_type c) override;
	};
}
