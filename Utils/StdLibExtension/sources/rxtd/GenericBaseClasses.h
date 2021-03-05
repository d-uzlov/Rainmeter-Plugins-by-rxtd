// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd {
	class VirtualDestructorBase {
	public:
		virtual ~VirtualDestructorBase() = default;
	};

	class MovableOnlyBase {
	public:
		MovableOnlyBase() = default;
		~MovableOnlyBase() = default;

		MovableOnlyBase(MovableOnlyBase&& other) = default;
		MovableOnlyBase(const MovableOnlyBase& other) = delete;
		MovableOnlyBase& operator=(MovableOnlyBase&& other) = default;
		MovableOnlyBase& operator=(const MovableOnlyBase& other) = delete;
	};

	class NonMovableBase {
	public:
		NonMovableBase() = default;
		~NonMovableBase() = default;

		NonMovableBase(NonMovableBase&& other) = delete;
		NonMovableBase(const NonMovableBase& other) = delete;
		NonMovableBase& operator=(NonMovableBase&& other) = delete;
		NonMovableBase& operator=(const NonMovableBase& other) = delete;
	};
}
