/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#pragma warning(push)

#pragma warning(disable : 4623)
#pragma warning(disable : 4626)
#pragma warning(disable : 5027)

namespace rxtd::std_fixes {
	// from std::visit documentation
	template<class... Ts>
	struct OverloadedVisitor : Ts... {
		using Ts::operator()...;
	};

	template<class... Ts>
	OverloadedVisitor(Ts ...)->OverloadedVisitor<Ts...>;
}

#pragma warning(pop)