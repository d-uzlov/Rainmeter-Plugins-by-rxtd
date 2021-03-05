// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

#include <ostream>

namespace rxtd::std_fixes {
	namespace cistring_details {
		template<typename C, typename T, T(*ToUpper)(T)>
		struct ci_char_traits : public std::char_traits<C> {
			using char_type = C;

			static bool eq(char_type c1, char_type c2) {
				return ToUpper(c1) == ToUpper(c2);
			}

			static bool ne(char_type c1, char_type c2) {
				return ToUpper(c1) != ToUpper(c2);
			}

			static bool lt(char_type c1, char_type c2) {
				return ToUpper(c1) < ToUpper(c2);
			}

			static int compare(const char_type* s1, const char_type* s2, size_t n) {
				while (n-- != 0) {
					auto c1 = ToUpper(*s1);
					auto c2 = ToUpper(*s2);
					if (c1 == c2) {
						++s1;
						++s2;
						continue;
					}
					return c1 < c2 ? -1 : 1;
				}

				return 0;
			}

			static const char_type* find(const char_type* s, size_t n, char_type a) {
				for (size_t i = 0; i < n; ++i) {
					if (ToUpper(s[i]) == ToUpper(a)) {
						return s + i;
					}
				}
				return nullptr;
			}
		};

		template<typename C, typename T, T(*ToUpper)(T), class Alloc = std::allocator<C>>
		using case_insensitive_string = StringBase<C, ci_char_traits<C, T, ToUpper>, Alloc>;

		template<typename C, typename T, T(*ToUpper)(T)>
		using case_insensitive_string_view = StringViewBase<C, ci_char_traits<C, T, ToUpper>>;
	}

	using istring = cistring_details::case_insensitive_string<wchar_t, wint_t, towupper>;
	using isview = cistring_details::case_insensitive_string_view<wchar_t, wint_t, towupper>;
	
	std::wostream& operator <<(std::wostream& stream, isview view);
}
