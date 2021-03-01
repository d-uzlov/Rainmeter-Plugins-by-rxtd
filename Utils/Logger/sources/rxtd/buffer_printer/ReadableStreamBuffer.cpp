/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ReadableStreamBuffer.h"

using rxtd::buffer_printer::ReadableStreamBuffer;

ReadableStreamBuffer::ReadableStreamBuffer(ReadableStreamBuffer&& other) noexcept:
	std::basic_streambuf<wchar_t>(other),
	buffer(std::move(other.buffer)) {}

ReadableStreamBuffer& ReadableStreamBuffer::operator=(ReadableStreamBuffer&& other) noexcept {
	if (this == &other)
		return *this;
	std::basic_streambuf<wchar_t>::operator =(other);
	buffer = std::move(other.buffer);
	return *this;
}

rxtd::sview ReadableStreamBuffer::getBuffer() {
	const index size = pptr() - pbase();
	return { buffer.data(), size };
}

void ReadableStreamBuffer::resetPointers() {
	char_type* buf = buffer.data();
	// buffer.size() - 1 because we need size for '\0' symbol at the end
	setp(buf, buf + buffer.size() - 1);
}

void ReadableStreamBuffer::appendEOL() {
	sputc(L'\0');
}

std::wstreambuf::int_type ReadableStreamBuffer::overflow(int_type c) {
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
