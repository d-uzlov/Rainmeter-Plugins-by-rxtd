/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <charconv>

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

		SubstringViewInfo substr(index subOffset, index subLength = std::numeric_limits<index>::max()) const;

		bool operator <(const SubstringViewInfo& other) const;
	};

	class StringUtils {
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

		static string trimCopy(const wchar_t *str) {
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
		static void substringInplace(std::basic_string<wchar_t, CT, A>& str, index begin, index count = string::npos) {
			if (begin > 0) {
				str.erase(0, begin);
			}
			if (count <= index(str.length())) {
				str.erase(count);
			}
		}

		static index parseInt(sview view) {
			char buffer[80];

			view = trim(view);
			bool hex = false;
			if ((startsWith(view, sview { L"0x" }) || startsWith(view, sview { L"0X" }))) {
				hex = true;
				view = view.substr(2);
			} else if (startsWith(view, sview { L"+" })) {
				view = view.substr(1);
			}

			if (view.length() > 0 && !iswdigit(view.front())) {
				return 0;
			}

			index size = std::min(view.length(), sizeof(buffer));
			for (index i = 0; i < size; ++i) {
				const auto wc = view[i];
				if (wc > std::numeric_limits<uint8_t>::max()) {
					buffer[i] = '\0';
					size = i;
					break;
				}
				buffer[i] = static_cast<char>(wc);
			}

			index result = 0;
			const int base = hex ? 16 : 10;
			std::from_chars(buffer, buffer + size, result, base);
			return result;
		}

		static double parseFloat(sview view) {
			char buffer[80];

			view = trim(view);
			if (startsWith(view, sview { L"+" })) {
				view = view.substr(1);
			}

			if (view.empty() || !iswdigit(view.front())) {
				return 0;
			}

			index size = std::min(view.length(), sizeof(buffer));
			for (index i = 0; i < size; ++i) {
				const auto wc = view[i];
				if (wc > std::numeric_limits<uint8_t>::max()) {
					buffer[i] = '\0';
					size = i;
					break;
				}
				buffer[i] = static_cast<char>(wc);
			}

			double result = 0;
			std::from_chars(buffer, buffer + size, result);
			return result;
		}

		static double parseFractional(sview view) {
			char buffer[80];

			view = trim(view);
			if (view.empty() || !iswdigit(view.front())) {
				return 0;
			}

			buffer[0] = '0';
			buffer[1] = '.';

			index size = std::min(view.length(), sizeof(buffer));
			for (index i = 0; i < size; ++i) {
				const auto wc = view[i];
				if (wc > std::numeric_limits<uint8_t>::max()) {
					buffer[i + 2] = '\0';
					size = i;
					break;
				}
				buffer[i + 2] = static_cast<char>(wc);
			}

			double result = 0;
			std::from_chars(buffer, buffer + size + 2, result);
			return result;
		}
	};
}