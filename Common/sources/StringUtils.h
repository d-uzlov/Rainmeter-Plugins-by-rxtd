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
	using namespace std::string_view_literals;

	// This class allows you to seamlessly replace source of a string view.
	// Very helpful when you have an array of string views that you don't want to update on source change.
	class SubstringViewInfo {
		index offset{ };
		index length{ };

	public:
		SubstringViewInfo() = default;

		SubstringViewInfo(index offset, index length) : offset(offset), length(length) {
		}

		[[nodiscard]]
		bool empty() const {
			return length == 0;
		}

		sview makeView(const wchar_t* base) const {
			return sview(base + offset, length);
		}

		template <typename CharTraits>
		[[nodiscard]]
		std::basic_string_view<wchar_t, CharTraits> makeView(std::basic_string_view<wchar_t, CharTraits> base) const {
			return std::basic_string_view<wchar_t, CharTraits>(base.data() + offset, length);
		}

		template <typename CharTraits, typename Allocator>
		[[nodiscard]]
		std::basic_string_view<wchar_t, CharTraits> makeView(
			const std::basic_string<wchar_t, CharTraits, Allocator>& base) const {
			return std::basic_string_view<wchar_t, CharTraits>(base.data() + offset, length);
		}

		[[nodiscard]]
		sview makeView(const std::vector<wchar_t>& base) const {
			return makeView(sview{ base.data(), base.size() });
		}

		[[nodiscard]]
		SubstringViewInfo substr(index subOffset, index subLength = std::numeric_limits<index>::max()) const;

		[[nodiscard]]
		bool operator <(const SubstringViewInfo& other) const;
	};

	class StringUtils {
	public:
		template <typename CharTraits>
		static std::basic_string_view<wchar_t, CharTraits> trim(std::basic_string_view<wchar_t, CharTraits> view) {
			const auto begin = view.find_first_not_of(L" \t");
			if (begin == sview::npos) {
				return { };
			}

			const auto end = view.find_last_not_of(L" \t"); // always valid if find_first_not_of succeeded

			return { view.data() + begin, end - begin + 1 };
		}

		[[nodiscard]]
		static sview trim(const wchar_t* view) {
			return trim(sview{ view });
		}

		[[nodiscard]]
		static SubstringViewInfo trimInfo(const wchar_t* base, SubstringViewInfo viewInfo);

		[[nodiscard]]
		static SubstringViewInfo trimInfo(sview source, SubstringViewInfo viewInfo) {
			return trimInfo(source.data(), viewInfo);
		}

		template <typename CharTraits, typename Allocator>
		static void lowerInplace(std::basic_string<wchar_t, CharTraits, Allocator>& str) {
			for (auto& c : str) {
				c = std::towlower(c);
			}
		}

		template <typename CharTraits, typename Allocator>
		static void upperInplace(std::basic_string<wchar_t, CharTraits, Allocator>& str) {
			for (auto& c : str) {
				c = std::towupper(c);
			}
		}

		template <typename CharTraits>
		[[nodiscard]]
		static string copyLower(std::basic_string_view<wchar_t, CharTraits> str) {
			std::basic_string<wchar_t, CharTraits> buffer{ str };
			lowerInplace(buffer);

			return buffer;
		}

		template <typename CharTraits>
		[[nodiscard]]
		static string copyUpper(std::basic_string_view<wchar_t, CharTraits> str) {
			std::basic_string<wchar_t, CharTraits> buffer{ str };
			upperInplace(buffer);

			return buffer;
		}

		template <typename CharTraits, typename Allocator>
		static void trimInplace(std::basic_string<wchar_t, CharTraits, Allocator>& str) {
			const auto firstNotSpace = std::find_if_not(str.begin(), str.end(), [](wint_t c) {
				return std::iswspace(c);
			});
			str.erase(str.begin(), firstNotSpace);
			const auto lastNotSpace = std::find_if_not(str.rbegin(), str.rend(), [](wint_t c) {
				return std::iswspace(c);
			}).base();
			str.erase(lastNotSpace, str.end());
		}

		template <typename CharTraits>
		[[nodiscard]]
		static std::basic_string<wchar_t, CharTraits> trimCopy(std::basic_string_view<wchar_t, CharTraits> str) {
			// from stackoverflow
			const auto firstNotSpace = std::find_if_not(
				str.begin(),
				str.end(),
				[](wint_t c) { return std::iswspace(c); }
			);
			const auto lastNotSpace = std::find_if_not(
				str.rbegin(),
				sview::const_reverse_iterator(firstNotSpace),
				[](wint_t c) { return std::iswspace(c); }
			).base();

			return { firstNotSpace, lastNotSpace };
		}

		[[nodiscard]]
		static string trimCopy(const wchar_t* str) {
			return trimCopy(sview{ str });
		}


		template <typename CharTraits>
		[[nodiscard]]
		static bool checkEndsWith(
			std::basic_string_view<wchar_t, CharTraits> str,
			std::basic_string_view<wchar_t, CharTraits> suffix
		) {
			// from stackoverflow
			return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
		}

		template <typename CharTraits>
		[[nodiscard]]
		static bool checkEndsWith(
			std::basic_string_view<wchar_t, CharTraits> str,
			const wchar_t* prefix
		) {
			return checkEndsWith(str, std::basic_string_view<wchar_t, CharTraits>{ prefix });
		}

		template <typename CharTraits>
		[[nodiscard]]
		static bool checkStartsWith(
			std::basic_string_view<wchar_t, CharTraits> str,
			std::basic_string_view<wchar_t, CharTraits> prefix
		) {
			// from stackoverflow
			return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
		}

		template <typename CharTraits>
		[[nodiscard]]
		static bool checkStartsWith(
			std::basic_string_view<wchar_t, CharTraits> str,
			const wchar_t* prefix
		) {
			return checkStartsWith(str, std::basic_string_view<wchar_t, CharTraits>{ prefix });
		}

		template <typename CharTraits, typename Allocator>
		static void substringInplace(
			std::basic_string<wchar_t, CharTraits, Allocator>& str,
			index begin,
			index count = string::npos
		) {
			const index endRemoveStart = begin + count;
			if (endRemoveStart <= index(str.length())) {
				str.erase(endRemoveStart);
			}
			if (begin > 0) {
				str.erase(0, begin);
			}
		}

		[[nodiscard]]
		static index parseInt(sview view);

		[[nodiscard]]
		static double parseFloat(sview view);

		[[nodiscard]]
		static double parseFractional(sview view);
	};
}
