/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	class SubstringViewInfo {
		index offset { };
		index length { };

	public:
		SubstringViewInfo();
		SubstringViewInfo(index offset, index length);

		bool empty() const;

		sview makeView(const wchar_t* base) const;

		template<typename CT>
		std::basic_string_view<wchar_t, CT> makeView(std::basic_string_view<wchar_t, CT> base) const {
			return std::basic_string_view<wchar_t, CT>(base.data() + offset, length);
		}

		template<typename CT, typename A>
		std::basic_string_view<wchar_t, CT> makeView(const std::basic_string<wchar_t, CT, A>& base) const {
			return std::basic_string_view<wchar_t, CT>(base.data() + offset, length);
		}

		sview makeView(const std::vector<wchar_t>& base) const;

		SubstringViewInfo substr(index subOffset, index subLength = sview::npos) const;

		bool operator <(const SubstringViewInfo& other) const;
	};

	class StringViewUtils {
	public:
		template<typename CT>
		static std::basic_string_view<wchar_t, CT> trim(std::basic_string_view<wchar_t, CT> view) {
			const auto begin = view.find_first_not_of(L" \t");
			if (begin == sview::npos) {
				return { };
			}

			const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

			return { view.data() + begin, end - begin + 1 };
		}

		static sview trim(const wchar_t* view) {
			return trim(sview { view });
		}

		static SubstringViewInfo trimInfo(const wchar_t* base, SubstringViewInfo viewInfo);

		static SubstringViewInfo trimInfo(sview source, SubstringViewInfo viewInfo);
	};

	class StringUtils {
	public:
		template<typename CT, typename A>
		static void lowerInplace(std::basic_string<wchar_t, CT, A>& str) {
			for (auto& c : str) {
				c = std::towlower(c);
			}
		}
		template<typename CT, typename A>
		static void upperInplace(std::basic_string<wchar_t, CT, A>& str) {
			for (auto& c : str) {
				c = std::towupper(c);
			}
		}

		template<typename CT>
		static string copyLower(std::basic_string_view<wchar_t, CT> str) {
			std::basic_string<wchar_t, CT> buffer { str };
			lowerInplace(buffer);

			return buffer;
		}

		template<typename CT>
		static string copyUpper(std::basic_string_view<wchar_t, CT> str) {
			std::basic_string<wchar_t, CT> buffer { str };
			upperInplace(buffer);

			return buffer;
		}

		template<typename CT, typename A>
		static void trimInplace(std::basic_string<wchar_t, CT, A>& str) {
			const auto firstNotSpace = std::find_if_not(str.begin(), str.end(), [](wint_t c) { return std::iswspace(c); });
			str.erase(str.begin(), firstNotSpace);
			const auto lastNotSpace = std::find_if_not(str.rbegin(), str.rend(), [](wint_t c) { return std::iswspace(c); }).base();
			str.erase(lastNotSpace, str.end());
		}

		template<typename CT>
		static std::basic_string<wchar_t, CT> trimCopy(std::basic_string_view<wchar_t, CT> str) {
			// from stackoverflow
			const auto firstNotSpace = std::find_if_not(str.begin(), str.end(), [](wint_t c) { return std::iswspace(c); });
			const auto lastNotSpace = std::find_if_not(str.rbegin(), sview::const_reverse_iterator(firstNotSpace), [](wint_t c) { return std::iswspace(c); }).base();

			return { firstNotSpace, lastNotSpace };
		}

		static string trimCopy(const wchar_t*str) {
			return trimCopy(sview { str });
		}


		template<typename CT>
		static bool endsWith(std::basic_string_view<wchar_t, CT> str, std::basic_string_view<wchar_t, CT> suffix) {
			// from stackoverflow
			return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
		}

		template<typename CT>
		static bool startsWith(std::basic_string_view<wchar_t, CT> str, std::basic_string_view<wchar_t, CT> prefix) {
			// from stackoverflow
			return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
		}

		template<typename CT, typename A>
		static void substringInplace(std::basic_string<wchar_t, CT, A>& str, index begin, index count) {
			if (begin > 0) {
				str.erase(0, begin);
			}
			if (count <= index(str.length())) {
				str.erase(count);
			}
		}

	};
}
