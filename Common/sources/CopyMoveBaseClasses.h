/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#ifndef COPY_MOVE_BASE_CLASSES_H
#define COPY_MOVE_BASE_CLASSES_H

namespace rxtd {
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

#endif
