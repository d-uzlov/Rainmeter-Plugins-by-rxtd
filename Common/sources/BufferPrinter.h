/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <optional>
#include <iomanip>

namespace rxtd::utils {
	using namespace std::literals::string_view_literals;

	template<typename E>
	typename std::enable_if<std::is_enum<E>::value, sview>::type
		getEnumName(E value) {
		return L"<unknown enum>"sv;
	}

	template <typename E>
	typename std::enable_if<std::is_enum<E>::value, void>::type
		writeEnum(std::wostream& stream, const E& e, sview options) {
		if (options == L"name"sv) {
			stream << getEnumName(e);
		} else {
			stream << std::underlying_type<E>::type(e);
		}
	}

	template <typename T>
	typename std::enable_if<std::is_integral<T>::value, void>::type
		writeIntegral(std::wostream& stream, T t, sview options) {
		if (options == L"error"sv) {
			stream << L"0x"sv;
			stream << std::setfill(L'0') << std::setw(sizeof(T) * 2) << std::hex;
			stream << t;
			return;
		}

		stream << t;
	}

	template <>
	void writeIntegral(std::wostream& stream, bool t, sview options);

	template <typename F>
	typename std::enable_if<std::is_floating_point<F>::value, void>::type
		writeFloat(std::wostream& stream, const F& t, sview options) {
		stream << t;
	}

	template <typename O>
	void writeObject(std::wostream& stream, const O& t, sview options) {
		stream << t;
	}

	/**
	 * Type-safe analogue of printf.
	 * Use format string and a list of arguments.
	 *
	 * Format string format: "smth1{options}smth2{}smth3"
	 *		smth1, smth2, smth3	� will be printed as is
	 *		{options}			� first argument will be written using "options" as argument to writeObject function
	 *		{}					� second argument will be written using "" as argument to writeObject function
	 *
	 * By default writeObject function calls operator<< on object. Specialize template to change this for your class.
	 * writeObject is specialized for few cases:
	 *	1. Integral types:
	 *		"error"		� print zero-padded value with "0x" prefix
	 *		""			� print number as is
	 *	2. Enums:
	 *		"name"		� use user-provided function "sview getEnumName(Enum value)"
	 *		""			� cast enum to number
	 *	3. Bool:
	 *		"number"	� cast bool to int
	 *		""			� print "true" or "false"
	 */
	class BufferPrinter {
		class ReadableOutputBuffer : public std::basic_streambuf<wchar_t> {
		private:
			std::vector<char_type> buffer;

		public:
			ReadableOutputBuffer();
			~ReadableOutputBuffer() = default;

			ReadableOutputBuffer(ReadableOutputBuffer&& other) noexcept;
			ReadableOutputBuffer& operator=(ReadableOutputBuffer&& other) noexcept;

			ReadableOutputBuffer(const ReadableOutputBuffer& other);
			ReadableOutputBuffer& operator=(const ReadableOutputBuffer& other);

			const char_type* getBuffer();
			void resetPointers();
			void appendEOL();

		protected:
			int_type overflow(int_type c) override;
		};

		ReadableOutputBuffer buffer;
		const wchar_t *formatString = nullptr;
		bool skipUnlistedArgs = true;

	public:
		explicit BufferPrinter() = default;
		~BufferPrinter() = default;

		BufferPrinter(BufferPrinter&& other) noexcept;
		BufferPrinter(const BufferPrinter& other) noexcept;
		BufferPrinter& operator=(const BufferPrinter& other) noexcept;
		BufferPrinter& operator=(BufferPrinter&& other) noexcept;

		void setSkipUnlistedArgs(bool value);

		template<typename... Args>
		void print(const wchar_t *formatString, const Args&... args) {
			buffer.resetPointers();
			append(formatString, args...);
		}

		template<typename... Args>
		void append(const wchar_t *formatString, const Args&... args) {
			this->formatString = formatString;
			writeToStream(args...);
			this->formatString = nullptr;
		}

		void reset() {
			buffer.resetPointers();
		}

		const wchar_t* getBufferPtr() {
			buffer.appendEOL();
			return buffer.getBuffer();
		}
	private:

		template<typename T, typename... Args>
		void writeToStream(const T& t, const Args&... args);

		void writeToStream();

		template<typename T, typename... Args>
		void writeUnlisted(const T& t, const Args&... args);

		void writeUnlisted();

		template <typename T>
		void writeType(std::wostream& stream, const T& t, sview options) {
			if constexpr (std::is_enum<T>::value) {
				writeEnum(stream, t, options);
			} else if constexpr (std::is_integral<T>::value) {
				writeIntegral(stream, t, options);
			} else if constexpr (std::is_floating_point<T>::value) {
				writeFloat(stream, t, options);
			} else {
				writeObject(stream, t, options);
			}
		}
	};

	/**
	 * Wrapper over a rainmeter skin handler.
	 * Used for type safety only.
	 */
	class Skin {
		void *const skin;

	public:
		explicit Skin(void* skin);
		void* getRawPointer() const;
	};

	template <typename T, typename ... Args>
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

			if (current == L'{') {
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

				if (view == L"!"sv) {
					stream << L'{';
				} else {
					option = view;
					break;
				}
			}

			formatString++;
		}

		if (!option.has_value()) {
			return;
		}

		writeType(stream, t, option.value());

		writeToStream(args...);
	}

	template <typename T, typename ... Args>
	void BufferPrinter::writeUnlisted(const T& t, const Args&... args) {
		std::wostream stream = std::wostream(&buffer);
		writeType(stream, t, L""sv);

		writeUnlisted(args...);
	}
}