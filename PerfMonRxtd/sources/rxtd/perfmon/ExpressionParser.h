/* 
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/expressions/ASTParser.h"

namespace rxtd::perfmon {
	class ExpressionParser : public common::expressions::ASTParser {
		using IndexType = common::expressions::ast_nodes::IndexType;
		using Lexer = common::expressions::Lexer;

	public:
		ExpressionParser();

	protected:
		std::optional<IndexType> parseCustom() override;
	};
}
