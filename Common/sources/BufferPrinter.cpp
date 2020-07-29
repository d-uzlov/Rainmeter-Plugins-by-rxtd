/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BufferPrinter.h"

#include "undef.h"

using namespace utils;

void utils::writeObject(std::wostream& stream, const Option& t, sview options) {
	stream << t.asString();
}

BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer(ReadableOutputBuffer&& other) noexcept:
	std::basic_streambuf<wchar_t>(other),
	buffer(std::move(other.buffer)) {
}

BufferPrinter::ReadableOutputBuffer& BufferPrinter::ReadableOutputBuffer::operator=(
	ReadableOutputBuffer&& other) noexcept {
	if (this == &other)
		return *this;
	std::basic_streambuf<wchar_t>::operator =(other);
	buffer = std::move(other.buffer);
	return *this;
}

const std::basic_streambuf<wchar_t>::char_type* BufferPrinter::ReadableOutputBuffer::getBuffer() {
	return buffer.data();
}

void BufferPrinter::ReadableOutputBuffer::resetPointers() {
	char_type* buf = buffer.data();
	// buffer.size() - 1 because we need size for '\0' symbol at the end
	setp(buf, buf + buffer.size() - 1);
}

void BufferPrinter::ReadableOutputBuffer::appendEOL() {
	sputc(L'\0');
}

std::basic_streambuf<wchar_t>::int_type BufferPrinter::ReadableOutputBuffer::overflow(int_type c) {
	// https://stackoverflow.com/a/51571896

	const index size = pptr() - pbase();
	const index newCapacity = std::max<index>(32, size * 2); // initial size will be 32
	buffer.resize(newCapacity);

	resetPointers();
	pbump(int(size));
	if (c != int_type(EOF)) {
		buffer[size] = c;
		pbump(1);
	}
	return c;
}

void BufferPrinter::setSkipUnlistedArgs(bool value) {
	skipUnlistedArgs = value;
}

void BufferPrinter::writeToStream() {
	std::wostream stream = std::wostream(&buffer);
	stream << formatString;
}

void BufferPrinter::writeUnlisted() { }


namespace rxtd::utils {

	template <>
	void writeIntegral(std::wostream& stream, bool t, sview options) {
		if (options == L"number"sv) {
			stream << index(t);
		} else {
			stream << (t ? L"true"sv : L"false"sv);
		}
	}
}

