// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

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
