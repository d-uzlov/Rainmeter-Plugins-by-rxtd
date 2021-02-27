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
		ReadableStreamBuffer(ReadableStreamBuffer&& other) noexcept :
			std::basic_streambuf<wchar_t>(other),
			buffer(std::move(other.buffer)) {}

		ReadableStreamBuffer& operator=(ReadableStreamBuffer&& other) noexcept {
			if (this == &other)
				return *this;
			std::basic_streambuf<wchar_t>::operator =(other);
			buffer = std::move(other.buffer);
			return *this;
		}

		ReadableStreamBuffer(const ReadableStreamBuffer& other) = default;
		ReadableStreamBuffer& operator=(const ReadableStreamBuffer& other) = default;

		sview getBuffer() {
			const index size = pptr() - pbase();
			return { buffer.data(), size };
		}

		void resetPointers() {
			char_type* buf = buffer.data();
			// buffer.size() - 1 because we need size for '\0' symbol at the end
			setp(buf, buf + buffer.size() - 1);
		}

		void appendEOL() {
			sputc(L'\0');
		}

	protected:
		int_type overflow(int_type c) override {
			// Source: https://stackoverflow.com/a/51571896

			const index size = pptr() - pbase();
			const index newCapacity = std::max<index>(32, size * 2); // initial size will be 32
			buffer.resize(static_cast<std::vector<char_type>::size_type>(newCapacity));

			resetPointers();
			pbump(static_cast<int>(size));
			if (c != static_cast<int_type>(EOF)) {
				buffer[static_cast<std::vector<char_type>::size_type>(size)] = c;
				pbump(1);
			}
			return c;
		}
	};
}
