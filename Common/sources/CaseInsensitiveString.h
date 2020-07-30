/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd {
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

		template <typename C, typename T, T(*ToUpper)(T)>
		using case_insensitive_string = std::basic_string<C, ci_char_traits<C, T, ToUpper>>;

		template <typename C, typename T, T(*ToUpper)(T)>
		using case_insensitive_string_view = std::basic_string_view<C, ci_char_traits<C, T, ToUpper>>;
	}

	using istring = cistring_details::case_insensitive_string<wchar_t, wint_t, towupper>;
	using isview = cistring_details::case_insensitive_string_view<wchar_t, wint_t, towupper>;

	inline namespace string_conversion {

		namespace conversion_details {
			struct caseInsensitiveObject { };
			struct caseSensitiveObject { };
			struct caseInsensitiveViewObject { };
			struct caseSensitiveViewObject { };
			struct stringCreationObject { };

		}
		inline struct {
			[[nodiscard]]
			struct conversion_details::caseInsensitiveObject operator()() const {
				return { };
			}
		} ciString { };

		[[nodiscard]]
		inline istring operator%(string&& str, conversion_details::caseInsensitiveObject) {
			istring result { reinterpret_cast<istring&&>(str) };
			return result;
		}

		inline struct {
			[[nodiscard]]
			struct conversion_details::caseSensitiveObject operator()() const {
				return { };
			}
		} csString { };

		[[nodiscard]]
		inline string operator%(istring&& str, conversion_details::caseSensitiveObject) {
			string result { reinterpret_cast<string&&>(str) };
			return result;
		}


		inline struct {
			[[nodiscard]]
			conversion_details::caseInsensitiveViewObject operator()() const {
				return { };
			}
		} ciView { };

		[[nodiscard]]
		inline isview operator%(sview view, conversion_details::caseInsensitiveViewObject) {
			return { view.data(), view.length() };
		}

		inline struct {
			[[nodiscard]]
			conversion_details::caseSensitiveViewObject operator()() const {
				return { };
			}
		} csView { };

		[[nodiscard]]
		inline sview operator%(isview view, conversion_details::caseSensitiveViewObject) {
			return { view.data(), view.length() };
		}


		inline struct {
			[[nodiscard]]
			struct conversion_details::stringCreationObject operator()() const {
				return { };
			}
		} own { };

		[[nodiscard]]
		inline string operator%(sview view, conversion_details::stringCreationObject) {
			return { view.data(), view.length() };
		}

		[[nodiscard]]
		inline istring operator%(isview view, conversion_details::stringCreationObject) {
			return { view.data(), view.length() };
		}

	}


	inline std::wostream& operator <<(std::wostream& stream, isview view) {
		return stream << (view % csView());
	}
}
