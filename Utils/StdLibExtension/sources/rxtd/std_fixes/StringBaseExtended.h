// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::std_fixes {

	//
	// Signed/unsigned mismatch is evil.
	// C++ standard library uses unsigned everywhere.
	// I can do little about it.
	// However, I can at least overload functions in std::basic_string
	// to take and return signed integers
	// instead of using static_cast million times in the rest of the project.
	//

	template<typename Elem, typename Traits = std::char_traits<Elem>, typename Alloc = std::allocator<Elem>>
	class StringBaseExtended;

	template<typename Elem, typename Traits = std::char_traits<Elem>>
	class StringViewBaseExtended : public std::basic_string_view<Elem, Traits> {
	public:
		using base = std::basic_string_view<Elem, Traits>;
		using base::base;
		using base::operator=;
		using view_type = std::basic_string_view<Elem, Traits>;
		using size_type = index;

		static constexpr auto npos{ static_cast<size_type>(base::npos) };

		StringViewBaseExtended(base b) noexcept : base(b) { }

		constexpr StringViewBaseExtended(const typename base::const_pointer ptr, const size_type count) noexcept : base(ptr, static_cast<typename base::size_type>(count)) { }

		constexpr StringViewBaseExtended(const typename base::const_pointer ptr, const int count) noexcept : StringViewBaseExtended(ptr, static_cast<size_type>(count)) { }

		StringViewBaseExtended(const StringBaseExtended<Elem, Traits>&) noexcept;

		[[nodiscard]]
		constexpr typename base::const_reference operator[](index offset) const noexcept {
			return this->base::operator[](static_cast<typename base::size_type>(offset));
		}


		[[nodiscard]]
		constexpr size_type find_first_not_of(const view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_first_not_of(Elem right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_not_of(const view_type right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_not_of(Elem right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_not_of(right, static_cast<typename base::size_type>(off)));
		}


		[[nodiscard]]
		constexpr size_type find_first_of(const view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_first_of(Elem right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_of(const view_type right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_of(Elem right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_of(right, static_cast<typename base::size_type>(off)));
		}


		[[nodiscard]]
		constexpr size_type find(view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find(const Elem ch, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find(ch, static_cast<typename base::size_type>(off)));
		}


		constexpr void remove_prefix(const size_type count) noexcept {
			base::remove_prefix(static_cast<typename base::size_type>(count));
		}

		constexpr void remove_suffix(const size_type count) noexcept {
			base::remove_suffix(static_cast<typename base::size_type>(count));
		}

		[[nodiscard]]
		constexpr StringViewBaseExtended substr(const size_type off = 0, size_type count = npos) const {
			return StringViewBaseExtended{ base::substr(static_cast<typename base::size_type>(off), static_cast<typename base::size_type>(count)) };
		}


		[[nodiscard]]
		constexpr size_type size() const noexcept {
			return static_cast<size_type>(base::size());
		}

		[[nodiscard]]
		constexpr size_type length() const noexcept {
			return static_cast<size_type>(base::length());
		}
	};


	template<typename Elem, typename Traits, typename Alloc>
	class StringBaseExtended : public std::basic_string<Elem, Traits, Alloc> {
	public:
		using base = std::basic_string<Elem, Traits, Alloc>;
		using base::base;
		using base::operator=;
		using base::operator+=;
		using base::operator[];
		using view_type = std::basic_string_view<Elem, Traits>;
		using base::operator std::basic_string_view<Elem, Traits>;
		using size_type = index;

		static constexpr auto npos{ static_cast<size_type>(base::npos) };


		StringBaseExtended(const base& b) : base(b) { }

		StringBaseExtended(const typename base::const_pointer ptr, const size_type count) noexcept : base(ptr, static_cast<typename base::size_type>(count)) { }

		operator StringViewBaseExtended<Elem, Traits>() {
			return view();
		}

		StringViewBaseExtended<Elem, Traits> view() {
			return StringViewBaseExtended<Elem, Traits>{ this->data(), this->size() };
		}


		[[nodiscard]]
		constexpr typename base::reference operator[](index offset) noexcept {
			return this->base::operator[](static_cast<typename base::size_type>(offset));
		}


		[[nodiscard]]
		constexpr typename base::const_reference operator[](index offset) const noexcept {
			return this->base::operator[](static_cast<typename base::size_type>(offset));
		}


		[[nodiscard]]
		constexpr size_type find_first_not_of(const view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_first_not_of(Elem right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_not_of(const view_type right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_not_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_not_of(Elem right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_not_of(right, static_cast<typename base::size_type>(off)));
		}


		[[nodiscard]]
		constexpr size_type find_first_of(const view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_first_of(Elem right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find_first_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_of(const view_type right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_of(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find_last_of(Elem right, const size_type off = npos) const noexcept {
			return static_cast<size_type>(base::find_last_of(right, static_cast<typename base::size_type>(off)));
		}


		[[nodiscard]]
		constexpr size_type find(view_type right, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find(right, static_cast<typename base::size_type>(off)));
		}

		[[nodiscard]]
		constexpr size_type find(const Elem ch, const size_type off = 0) const noexcept {
			return static_cast<size_type>(base::find(ch, static_cast<typename base::size_type>(off)));
		}


		[[nodiscard]]
		constexpr size_type size() const noexcept {
			return static_cast<size_type>(base::size());
		}

		[[nodiscard]]
		constexpr size_type length() const noexcept {
			return static_cast<size_type>(base::length());
		}
	};


	template<typename Elem, typename Traits>
	StringViewBaseExtended<Elem, Traits>::StringViewBaseExtended(const StringBaseExtended<Elem, Traits>& str) noexcept : base(str) { }
}

namespace std {
	template<>
	struct hash<rxtd::std_fixes::StringViewBaseExtended<wchar_t>> {
		size_t operator()(const rxtd::std_fixes::StringViewBaseExtended<wchar_t>& value) const noexcept {
			return hash<std::wstring_view>()(value);
		}
	};

	template<>
	struct hash<rxtd::std_fixes::StringBaseExtended<wchar_t>> {
		size_t operator()(const rxtd::std_fixes::StringBaseExtended<wchar_t>& value) const noexcept {
			return hash<std::wstring>()(value);
		}
	};
}
