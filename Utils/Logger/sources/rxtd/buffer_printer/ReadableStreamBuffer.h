// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
