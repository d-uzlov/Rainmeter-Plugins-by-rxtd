/* 
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/expression_parser/ASTParser.h"

namespace rxtd::perfmon {
	class ExpressionParser : public expression_parser::ASTParser {
		using IndexType = expression_parser::ast_nodes::IndexType;
		using Lexer = expression_parser::Lexer;

	public:
		ExpressionParser();

	protected:
		std::optional<IndexType> parseCustom() override;
	};
}
