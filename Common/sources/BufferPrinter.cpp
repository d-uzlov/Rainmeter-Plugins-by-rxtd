/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BufferPrinter.h"

#pragma warning(disable : 4458)

rxu::BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer() {
	buffer.resize(16);
	resetPointers();
}

rxu::BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer(ReadableOutputBuffer&& other) noexcept :
	std::basic_streambuf<wchar_t>(other),
	buffer(std::move(other.buffer)) { }

rxu::BufferPrinter::ReadableOutputBuffer& rxu::BufferPrinter::ReadableOutputBuffer::operator=(ReadableOutputBuffer&& other) noexcept {
	if (this == &other)
		return *this;

	buffer = std::move(other.buffer);
	std::basic_streambuf<wchar_t>::operator =(std::move(other));

	return *this;
}

rxu::BufferPrinter::ReadableOutputBuffer::ReadableOutputBuffer(const ReadableOutputBuffer& other) :
	std::basic_streambuf<wchar_t>(other),
	buffer(other.buffer) { }

rxu::BufferPrinter::ReadableOutputBuffer& rxu::BufferPrinter::ReadableOutputBuffer::operator=(
	const ReadableOutputBuffer& other) {
	if (this == &other)
		return *this;

	std::basic_streambuf<wchar_t>::operator =(other);
	buffer = other.buffer;

	return *this;
}

const std::basic_streambuf<wchar_t>::char_type* rxu::BufferPrinter::ReadableOutputBuffer::getBuffer() {
	return buffer.data();
}

void rxu::BufferPrinter::ReadableOutputBuffer::resetPointers() {
	char_type* buf = buffer.data();
	setp(buf, buf + buffer.size() - 1);
}

void rxu::BufferPrinter::ReadableOutputBuffer::appendEOL() {
	sputc(L'\0');
}

std::basic_streambuf<wchar_t>::int_type rxu::BufferPrinter::ReadableOutputBuffer::overflow(int_type c) {
	const auto oldSize = pptr() - pbase();
	buffer.resize(oldSize * 2);

	buffer[oldSize] = static_cast<char_type>(c);

	resetPointers();
	pbump(static_cast<int>(oldSize + 1));

	return c;
}

rxu::BufferPrinter::BufferPrinter(BufferPrinter&& other) noexcept :
	buffer(std::move(other.buffer)),
	formatString(other.formatString),
	skipUnlistedArgs(other.skipUnlistedArgs) { }

rxu::BufferPrinter::BufferPrinter(const BufferPrinter& other) noexcept :
	buffer(other.buffer),
	formatString(other.formatString),
	skipUnlistedArgs(other.skipUnlistedArgs) { }

rxu::BufferPrinter& rxu::BufferPrinter::operator=(const BufferPrinter& other) noexcept {
	if (this == &other)
		return *this;

	buffer = other.buffer;
	formatString = other.formatString;
	skipUnlistedArgs = other.skipUnlistedArgs;

	return *this;
}

rxu::BufferPrinter& rxu::BufferPrinter::operator=(BufferPrinter&& other) noexcept {
	if (this == &other)
		return *this;

	buffer = std::move(other.buffer);
	formatString = other.formatString;
	skipUnlistedArgs = other.skipUnlistedArgs;

	return *this;
}

void rxu::BufferPrinter::setSkipUnlistedArgs(bool value) {
	skipUnlistedArgs = value;
}

void rxu::BufferPrinter::writeToStream() {
	std::wostream stream = std::wostream(&buffer);
	stream << formatString;
}

void rxu::BufferPrinter::writeUnlisted() { }


namespace rxu {

	template <>
	void writeIntegral(std::wostream& stream, const bool& t, std::wstring_view options) {
		if (options == L"number"sv) {
			stream << static_cast<int>(t);
		} else {
			stream << (t ? L"true"sv : L"false"sv);
		}
	}

	// template <>
	// void writeObject(std::wostream& stream, const double& t, std::wstring_view options) {
	// 	stream << t;
	// }
	//
	// template <>
	// void writeObject<unsigned long long>(std::wostream& stream, const uint64_t& t, std::wstring_view options) {
	// }
}

