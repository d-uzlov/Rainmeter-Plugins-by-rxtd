/*
 * Implement a simple 1-dimensional array_view class
 *
 * (C) Copyright Marshall Clow 2016
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See http://www.boost.org/LICENSE_1_0.txt)
 *
 * Source:
 * https://github.com/mclow/snippets
 *
 * Modified by rxtd, 2019.
 */

// for some reason #pragma once didn't work for me here
#ifndef ARRAY_VIEW_ARRAY_SPAN_H
#define ARRAY_VIEW_ARRAY_SPAN_H

#include <cassert>
#include <iterator>
#include <limits>
#include <stdexcept>

template<typename T>
class array_view;

template<class T>
class array_span {
public:
	// types
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using reference = value_type&;
	using const_reference = const value_type&;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using size_type = ptrdiff_t;
	using difference_type = ptrdiff_t;

	constexpr array_span() noexcept : data_(nullptr), size_(0) { }

	constexpr array_span(value_type* p, size_type len) noexcept : data_(p), size_(len) { }

	template<size_type N>
	constexpr array_span(value_type (&arr)[N]) noexcept : data_(arr), size_(N) { }

	constexpr array_span(std::vector<value_type>& vec) : data_(vec.data()), size_(vec.size()) { }

	constexpr array_span(const array_span& rhs) noexcept = default;

	constexpr array_span& operator=(const array_span& rhs) noexcept = default;

	array_span(array_span&& other) noexcept = default;
	array_span& operator=(array_span&& other) noexcept = default;
	~array_span() = default;

	constexpr iterator begin() noexcept {
		return data_;
	}

	constexpr iterator end() noexcept {
		return data_ + size_;
	}

	constexpr const_iterator begin() const noexcept {
		return cbegin();
	}

	constexpr const_iterator end() const noexcept {
		return cend();
	}

	constexpr const_iterator cbegin() const noexcept {
		return data_;
	}

	constexpr const_iterator cend() const noexcept {
		return data_ + size_;
	}

	reverse_iterator rbegin() noexcept {
		return reverse_iterator(end());
	}

	reverse_iterator rend() noexcept {
		return reverse_iterator(begin());
	}

	const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator(cend());
	}

	const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator(cbegin());
	}

	const_reverse_iterator crbegin() const noexcept {
		return const_reverse_iterator(cend());
	}

	const_reverse_iterator crend() const noexcept {
		return const_reverse_iterator(cbegin());
	}

	constexpr size_type size() const noexcept {
		return size_;
	}

	constexpr size_type length() const noexcept {
		return size_;
	}

	constexpr size_type max_size() const noexcept {
		return std::numeric_limits<size_type>::max();
	}

	constexpr bool empty() const noexcept {
		return size_ == 0;
	}

	constexpr pointer data() noexcept {
		return data_;
	}

	constexpr const_pointer data() const noexcept {
		return data_;
	}

	constexpr reference operator[](size_type __pos) noexcept {
		return data_[__pos];
	}

	constexpr const_reference operator[](size_type __pos) const noexcept {
		return data_[__pos];
	}

	constexpr reference at(size_type __pos) noexcept(false) {
		if (__pos >= size())
			throw std::out_of_range("array_view1d::at");
		return data_[__pos];
	}

	constexpr const_reference at(size_type __pos) const noexcept(false) {
		if (__pos >= size())
			throw std::out_of_range("array_view1d::at");
		return data_[__pos];
	}

	constexpr reference front() noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::front");
		return data_[0];
	}

	constexpr const_reference front() const noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::front");
		return data_[0];
	}

	constexpr reference back() noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::back");
		return data_[size_ - 1];
	}

	constexpr const_reference back() const noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::back");
		return data_[size_ - 1];
	}

	constexpr void clear() noexcept {
		data_ = nullptr;
		size_ = 0;
	}

	constexpr void remove_prefix(size_type n) {
		assert(n <= size());
		data_ += n;
		size_ -= n;
	}

	constexpr void remove_suffix(size_type n) {
		assert(n <= size());
		size_ -= n;
	}

	constexpr void swap(array_span& other) noexcept {
		const value_type* p = data_;
		data_ = other.data_;
		other.data_ = p;

		size_type sz = size_;
		size_ = other.size_;
		other.size_ = sz;
	}

	constexpr bool operator==(array_span rhs) {
		if (size_ != rhs.size()) return false;
		for (size_type i = 0; i < size_; ++i)
			if (data_[i] != rhs[i])
				return false;
		return true;
	}

	constexpr bool operator!=(array_span rhs) {
		if (size_ != rhs.size()) return true;
		for (size_type i = 0; i < size_; ++i)
			if (data_[i] != rhs[i])
				return true;
		return false;
	}


	// Copyright (C) 2020 rxtd
	constexpr void transferToSpan(array_span<value_type> dest) const noexcept(false) {
		if (dest.size() != size())
			throw std::out_of_range("array_view1d::transferToSpan");
		auto myIter = begin();
		auto destIter = dest.begin();
		for (; myIter != end(); ++myIter, ++destIter) {
			*destIter = *myIter;
		}
	}

	// Copyright (C) 2020 rxtd
	constexpr void transferToVector(std::vector<value_type>& dest) const noexcept(false) {
		dest.resize(size());
		transferToSpan(array_span<value_type>{ dest });
	}

	// Copyright (C) 2020 rxtd
	constexpr void copyFrom(array_view<value_type> source);

	// Copyright (C) 2021 rxtd
	constexpr bool contains(const value_type& val) const {
		return std::find(begin(), end(), val) != end();
	}


private:
	pointer data_;
	size_type size_;
};


template<class T>
class array_view {
public:
	// types
	using value_type = T;
	using pointer = const value_type*;
	using const_pointer = const value_type*;
	using reference = const value_type&;
	using const_reference = const value_type&;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using reverse_iterator = const_reverse_iterator;
	using size_type = ptrdiff_t;
	using difference_type = ptrdiff_t;

	constexpr array_view() noexcept : data_(nullptr), size_(0) { }

	constexpr array_view(const value_type* p, size_type len) noexcept : data_(p), size_(len) { }

	template<size_type N>
	constexpr array_view(const value_type (&arr)[N]) noexcept : data_(arr), size_(N) { }

	constexpr array_view(const std::vector<value_type>& vec) : data_(vec.data()), size_(vec.size()) { }

	constexpr array_view(const array_view<value_type>& rhs) noexcept = default;

	constexpr array_view(const array_span<value_type>& rhs) noexcept : data_(rhs.data()), size_(rhs.size()) { }

	constexpr array_view& operator=(const array_view& rhs) noexcept = default;

	array_view(array_view&& other) noexcept = default;
	array_view& operator=(array_view&& other) noexcept = default;
	~array_view() = default;

	constexpr const_iterator begin() const noexcept {
		return cbegin();
	}

	constexpr const_iterator end() const noexcept {
		return cend();
	}

	constexpr const_iterator cbegin() const noexcept {
		return data_;
	}

	constexpr const_iterator cend() const noexcept {
		return data_ + size_;
	}

	const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator(cend());
	}

	const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator(cbegin());
	}

	const_reverse_iterator crbegin() const noexcept {
		return const_reverse_iterator(cend());
	}

	const_reverse_iterator crend() const noexcept {
		return const_reverse_iterator(cbegin());
	}

	constexpr size_type size() const noexcept {
		return size_;
	}

	constexpr size_type length() const noexcept {
		return size_;
	}

	constexpr size_type max_size() const noexcept {
		return std::numeric_limits<size_type>::max();
	}

	constexpr bool empty() const noexcept {
		return size_ == 0;
	}

	constexpr const_pointer data() const noexcept {
		return data_;
	}

	constexpr const_reference operator[](size_type __pos) const noexcept {
		return data_[__pos];
	}

	constexpr const_reference at(size_type __pos) const noexcept(false) {
		if (__pos >= size())
			throw std::out_of_range("array_view1d::at");
		return data_[__pos];
	}

	constexpr const_reference front() const noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::front");
		return data_[0];
	}

	constexpr const_reference back() const noexcept(false) {
		if (empty())
			throw std::out_of_range("array_view1d::back");
		return data_[size_ - 1];
	}

	constexpr void clear() noexcept {
		data_ = nullptr;
		size_ = 0;
	}

	constexpr void remove_prefix(size_type n) {
		assert(n <= size());
		data_ += n;
		size_ -= n;
	}

	constexpr void remove_suffix(size_type n) {
		assert(n <= size());
		size_ -= n;
	}

	constexpr void swap(array_view& other) noexcept {
		const value_type* p = data_;
		data_ = other.data_;
		other.data_ = p;

		size_type sz = size_;
		size_ = other.size_;
		other.size_ = sz;
	}

	constexpr bool operator==(array_view rhs) {
		if (size_ != rhs.size()) return false;
		for (size_type i = 0; i < size_; ++i)
			if (data_[i] != rhs[i])
				return false;
		return true;
	}

	constexpr bool operator!=(array_view rhs) {
		if (size_ != rhs.size()) return true;
		for (size_type i = 0; i < size_; ++i)
			if (data_[i] != rhs[i])
				return true;
		return false;
	}


	// Copyright (C) 2020 rxtd
	constexpr void transferToSpan(array_span<value_type> dest) const noexcept(false) {
		if (dest.size() != size())
			throw std::out_of_range("array_view1d::transferToSpan");
		auto myIter = begin();
		auto destIter = dest.begin();
		for (; myIter != end(); ++myIter, ++destIter) {
			*destIter = *myIter;
		}
	}

	// Copyright (C) 2020 rxtd
	constexpr void transferToVector(std::vector<value_type>& dest) const noexcept(false) {
		dest.resize(size());
		transferToSpan(array_span<T>{ dest });
	}

	// Copyright (C) 2021 rxtd
	constexpr bool contains(const value_type& val) const {
		return std::find(begin(), end(), val) != end();
	}

private:
	const_pointer data_;
	size_type size_;
};


template<class T>
constexpr void array_span<T>::copyFrom(array_view<value_type> source) {
	source.transferToSpan(*this);
}

#endif
