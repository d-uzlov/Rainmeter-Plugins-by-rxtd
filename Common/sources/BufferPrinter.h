/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <optional>
#include <iomanip>

#include "StringUtils.h"

namespace rxtd::utils {
	using ::operator<<;

	using FormattingFunctionType = std::optional<sview>(*)(index);
	// adds a dynamically resolved function that will be called by writeType<integer>()
	// when writeType:options match :name
	// :name must start with symbol 'f:'
	// :name must have static lifetime
	void registerFormattingFunction(sview name, FormattingFunctionType function);
	FormattingFunctionType findFormattingFunction(sview name);

	template<typename E>
	typename std::enable_if<std::is_enum<E>::value, sview>::type
	getEnumName(E value) {
		return L"<unknown enum>";
	}

	template<typename T>
	void writeType(std::wostream& stream, const T& t, sview options) {
		if constexpr (std::is_enum<T>::value) {
			if (options == L"name") {
				stream << getEnumName(t);
			} else {
				stream << std::underlying_type<T>::type(t);
			}
			return;
		} else if constexpr (std::is_integral<T>::value) {
			if (options == L"hex") {
				stream << L"0x";
				stream << std::setfill(L'0') << std::setw(sizeof(T) * 2) << std::hex;
				stream << t;
				return;
			}
			if (StringUtils::checkStartsWith(options, L"f:")) {
				auto fun = findFormattingFunction(options);
				if (fun == nullptr) {
					writeType(stream, t, L"hex");
					return;
				}
				auto valueOpt = fun(t);
				if (valueOpt.has_value()) {
					stream << valueOpt.value();
				} else {
					writeType(stream, t, L"hex");
				}
				return;
			}
			stream << t;
			return;
		} else {
			stream << t;
		}
	}

	template<>
	inline void writeType(std::wostream& stream, const bool& t, sview options) {
		if (options == L"number") {
			stream << index(t);
		} else {
			stream << (t ? L"true" : L"false");
		}
	}

	template<typename T>
	void writeType(std::wostream& stream, const std::vector<T>& vec, sview options) {
		stream << L'[';
		if (!vec.empty()) {
			bool first = true;
			for (const auto& value : vec) {
				if (first) {
					first = false;
				} else {
					stream << L", ";
				}
				stream << value;
			}
		}
		stream << L']';
	}

	/**
	 * Type-safe analogue of printf.
	 * Use format string and a list of arguments.
	 *
	 * Format string format: "smth1{options}smth2{}smth3"
	 *		smth1, smth2, smth3	— will be printed as is
	 *		{options}			— first argument will be written using "options" as argument to writeType function
	 *		{}					— second argument will be written using "" as argument to writeType function
	 *
	 * By default writeType function calls operator<< on object. Specialize template to change this for your class.
	 * writeType is specialized for few cases:
	 *	1. Integral types:
	 *		"error"		— print zero-padded value with "0x" prefix
	 *		""			— print number as is
	 *	2. Enums:
	 *		"name"		— use user-provided function "sview getEnumName(Enum value)"
	 *		""			— cast enum to number
	 *	3. Bool:
	 *		"number"	— cast bool to int
	 *		""			— print "true" or "false"
	 */
	class BufferPrinter {
		class ReadableOutputBuffer : public std::basic_streambuf<wchar_t> {
			std::vector<char_type> buffer;

		public:
			ReadableOutputBuffer() = default;
			~ReadableOutputBuffer() = default;

			// these constructors must be implemented explicitly because std::basic_streambuf is not movable,
			// so no default functions can be generated
			ReadableOutputBuffer(ReadableOutputBuffer&& other) noexcept;
			ReadableOutputBuffer& operator=(ReadableOutputBuffer&& other) noexcept;

			ReadableOutputBuffer(const ReadableOutputBuffer& other) = default;
			ReadableOutputBuffer& operator=(const ReadableOutputBuffer& other) = default;

			sview getBuffer();
			void resetPointers();
			void appendEOL();

		protected:
			int_type overflow(int_type c) override;
		};

		ReadableOutputBuffer buffer;
		const wchar_t* formatString = nullptr;
		bool skipUnlistedArgs = true;

	public:
		void setSkipUnlistedArgs(bool value) {
			skipUnlistedArgs = value;
		}

		template<typename... Args>
		void print(string formatString, const Args&... args) {
			print(formatString.c_str(), args...);
		}

		template<typename T>
		void print(T arg) {
			print(L"{}", arg);
		}

		template<typename... Args>
		void print(const wchar_t* formatString, const Args&... args) {
			buffer.resetPointers();
			append(formatString, args...);
		}

		template<typename... Args>
		void append(const wchar_t* formatString, const Args&... args) {
			this->formatString = formatString;
			writeToStream(args...);
			this->formatString = nullptr;
		}

		void reset() {
			buffer.resetPointers();
		}

		[[nodiscard]]
		const wchar_t* getBufferPtr() {
			buffer.appendEOL();
			return buffer.getBuffer().data();
		}

		[[nodiscard]]
		sview getBufferView() {
			buffer.appendEOL();

			return buffer.getBuffer();
		}

	private:
		template<typename T, typename... Args>
		void writeToStream(const T& t, const Args&... args);

		void writeToStream();

		template<typename T, typename... Args>
		void writeUnlisted(const T& t, const Args&... args);

		void writeUnlisted() { }
	};

	template<typename T, typename ... Args>
	void BufferPrinter::writeToStream(const T& t, const Args&... args) {

		auto begin = formatString;
		std::optional<sview> option;

		std::wostream stream = std::wostream(&buffer);

		auto current = formatString[0];
		while (true) {
			current = formatString[0];

			if (current == L'\0') {
				stream << begin;

				if (!skipUnlistedArgs) {
					writeUnlisted(t, args...);
				}
				return;
			}

			if (current != L'{') {
				formatString++;
				continue;
			}

			// current == L'{'
			stream << sview(begin, formatString - begin);

			formatString++;
			begin = formatString;
			auto end = begin;
			while (true) {
				current = formatString[0];

				if (current == L'\0') {
					stream << L'{';
					stream << begin;

					if (!skipUnlistedArgs) {
						writeUnlisted(t, args...);
					}
					return;
				}

				if (current == L'}') {
					end = formatString;
					formatString++;
					break;
				}
				formatString++;
			}

			const auto view = sview(begin, end - begin);

			if (view == L"!") {
				stream << L'{';
			} else {
				option = view;
				break;
			}

			formatString++;
		}

		if (!option.has_value()) {
			return;
		}

		writeType(stream, t, option.value());

		writeToStream(args...);
	}

	template<typename T, typename ... Args>
	void BufferPrinter::writeUnlisted(const T& t, const Args&... args) {
		std::wostream stream = std::wostream(&buffer);
		writeType(stream, t, {});

		writeUnlisted(args...);
	}
}
