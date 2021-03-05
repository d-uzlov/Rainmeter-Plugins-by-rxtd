// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

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
