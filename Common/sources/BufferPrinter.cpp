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

BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer() {
	buffer.resize(16);
	resetPointers();
}

BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer(ReadableOutputBuffer&& other) noexcept :
	std::basic_streambuf<wchar_t>(other),
	buffer(std::move(other.buffer)) { }

BufferPrinter::ReadableOutputBuffer& BufferPrinter::ReadableOutputBuffer::operator=(ReadableOutputBuffer&& other) noexcept {
	if (this == &other)
		return *this;

	buffer = std::move(other.buffer);
	std::basic_streambuf<wchar_t>::operator =(std::move(other));

	return *this;
}

BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer(const ReadableOutputBuffer& other) :
	std::basic_streambuf<wchar_t>(other),
	buffer(other.buffer) { }

BufferPrinter::ReadableOutputBuffer& BufferPrinter::ReadableOutputBuffer::operator=(
	const ReadableOutputBuffer& other) {
	if (this == &other)
		return *this;

	std::basic_streambuf<wchar_t>::operator =(other);
	buffer = other.buffer;

	return *this;
}

const std::basic_streambuf<wchar_t>::char_type* BufferPrinter::ReadableOutputBuffer::getBuffer() {
	return buffer.data();
}

void BufferPrinter::ReadableOutputBuffer::resetPointers() {
	char_type* buf = buffer.data();
	setp(buf, buf + buffer.size() - 1);
}

void BufferPrinter::ReadableOutputBuffer::appendEOL() {
	sputc(L'\0');
}

std::basic_streambuf<wchar_t>::int_type BufferPrinter::ReadableOutputBuffer::overflow(int_type c) {
	const auto oldSize = pptr() - pbase();
	buffer.resize(oldSize * 2);

	buffer[oldSize] = static_cast<char_type>(c);

	resetPointers();
	pbump(int(oldSize) + 1);

	return c;
}

BufferPrinter::BufferPrinter(BufferPrinter&& other) noexcept :
	buffer(std::move(other.buffer)),
	formatString(other.formatString),
	skipUnlistedArgs(other.skipUnlistedArgs) { }

BufferPrinter::BufferPrinter(const BufferPrinter& other) noexcept :
	buffer(other.buffer),
	formatString(other.formatString),
	skipUnlistedArgs(other.skipUnlistedArgs) { }

BufferPrinter& BufferPrinter::operator=(const BufferPrinter& other) noexcept {
	if (this == &other)
		return *this;

	buffer = other.buffer;
	formatString = other.formatString;
	skipUnlistedArgs = other.skipUnlistedArgs;

	return *this;
}

BufferPrinter& BufferPrinter::operator=(BufferPrinter&& other) noexcept {
	if (this == &other)
		return *this;

	buffer = std::move(other.buffer);
	formatString = other.formatString;
	skipUnlistedArgs = other.skipUnlistedArgs;

	return *this;
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

