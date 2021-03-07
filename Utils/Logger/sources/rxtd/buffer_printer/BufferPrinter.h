// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <iomanip>

#include "ReadableStreamBuffer.h"

//
// I created BufferPrinter class long time ago as an exercise for variadic templates.
// It doesn't have any real advantages compared to, for example, the famous fmt library
// but it works, I don't really need more functions then BufferPrinter supports,
// so I don't see any reason to get rid of it.
//
namespace rxtd::buffer_printer {
	template<typename E>
	sview getEnumName(E value) {
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
			stream << t;
			return;
		} else {
			stream << t;
		}
	}

	template<>
	inline void writeType(std::wostream& stream, const bool& t, sview options) {
		if (options == L"number") {
			stream << static_cast<index>(t);
		} else {
			stream << (t ? L"true" : L"false");
		}
	}

	template<typename T>
	void writeType(std::wostream& stream, const std::vector<T>& array, sview options) {
		writeType(stream, array_view<T>{ array }, options);
	}

	template<typename T>
	void writeType(std::wostream& stream, array_span<T> array, sview options) {
		writeType(stream, array_view<T>{ array }, options);
	}

	template<typename T>
	void writeType(std::wostream& stream, array_view<T> array, sview options) {
		const bool brackets = options != L"no-brackets";
		if (brackets) stream << L'[';
		if (!array.empty()) {
			bool first = true;
			for (const auto& value : array) {
				if (first) {
					first = false;
				} else {
					stream << L", ";
				}
				stream << value;
			}
		}
		if (brackets) stream << L']';
	}

	//
	// Type-safe analogue of printf.
	// Use format string and a list of arguments.
	//
	// For example:
	//    print("Hello, {}!", "world") will print "Hello, world!"
	//    print("0xff == {hex} is {}", 200, 0xff == 200) will print "0xff == 0xc8 is false"
	//
	// Format string format: "smth1{options}smth2{}smth3"
	//		smth1, smth2, smth3	— will be printed as is
	//		{options}			— first argument will be written using "options" as argument to writeType function
	//		{}					— second argument will be written using "" as argument to writeType function
	//
	// By default writeType function calls operator<< on object. Specialize template to change this for your class.
	// Signature: void writeType(std::wostream&, const Type&, sview options)
	//
	// writeType is specialized for few cases:
	//	• Integral types: use "{hex}" to print number in hexadecimal with "0x" prefix
	//	• Enums: use "{name}" to call function getEnumName(EnumType), which must be provided by user.
	//		Default getEnumName always returns "<unknown enum>".
	//	• Bool: use "{number}" to print as 0 or 1. Default behaviour is printing "true" or "false"
	//	• std::vector: no options, prints [value1, value2] using operator<<(std::owstream) on vector type
	//
	class BufferPrinter {
		ReadableStreamBuffer buffer;
		const wchar_t* formatStringCurrent = nullptr;
		const wchar_t* formatStringEnd = nullptr;
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
		void append(sview formatString, const Args&... args) {
			this->formatStringCurrent = formatString.data();
			this->formatStringEnd = formatString.data() + formatString.size();
			writeToStream(args...);
			this->formatStringCurrent = nullptr;
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
			auto result = buffer.getBuffer();
			result.remove_suffix(1);
			return result;
		}

	private:
		// I'm not proud of this function but it works, so I just don't touch it
		template<typename T, typename... Args>
		void writeToStream(const T& t, const Args&... args) {
			auto begin = formatStringCurrent;
			std::optional<sview> option;

			std::wostream stream = std::wostream(&buffer);

			while (true) {
				auto current = formatStringCurrent[0];

				if (formatStringCurrent == formatStringEnd || current == L'\0') {
					stream << begin;

					writeUnlisted(t, args...);
					return;
				}

				if (current != L'{') {
					formatStringCurrent++;
					continue;
				}

				// current == L'{'
				stream << sview(begin, formatStringCurrent - begin);

				formatStringCurrent++;
				begin = formatStringCurrent;
				auto end = begin;
				while (true) {
					current = formatStringCurrent[0];

					if (current == L'\0') {
						stream << L'{';
						stream << begin;

						writeUnlisted(t, args...);
						return;
					}

					if (current == L'}') {
						end = formatStringCurrent;
						formatStringCurrent++;
						break;
					}
					formatStringCurrent++;
				}

				const auto view = sview(begin, end - begin);

				if (view == L"!") {
					stream << L'{';
				} else {
					option = view;
					break;
				}

				formatStringCurrent++;
			}

			if (!option.has_value()) {
				return;
			}

			writeType(stream, t, option.value());

			writeToStream(args...);
		}

		void writeToStream() {
			std::wostream stream = std::wostream(&buffer);
			stream << formatStringCurrent;
		}

		template<typename T, typename... Args>
		void writeUnlisted(const T& t, const Args&... args) {
			if (skipUnlistedArgs) {
				return;
			}

			std::wostream stream = std::wostream(&buffer);
			writeType(stream, t, {});

			writeUnlisted(args...);
		}

		void writeUnlisted() { }
	};
}
