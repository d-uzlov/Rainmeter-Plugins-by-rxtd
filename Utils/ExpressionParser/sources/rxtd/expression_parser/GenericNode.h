// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

#include "OperatorNodes.h"
#include "rxtd/std_fixes/OverloadedVisitor.h"

namespace rxtd::expression_parser {
	struct GenericNode {
		std::variant<
			ast_nodes::ConstantNode,
			ast_nodes::BinaryOperatorNode,
			ast_nodes::PrefixOperatorNode,
			ast_nodes::PostfixOperatorNode,
			ast_nodes::WordNode,
			ast_nodes::FunctionNode,
			ast_nodes::CustomTerminalNode
		> value{};

		GenericNode() = default;

		template<typename T>
		GenericNode(T t) : value(std::move(t)) { }
		
		template<typename ... Visitor>
		auto visit(Visitor ... visitor) {
			return std::visit(std_fixes::OverloadedVisitor{ visitor... }, value);
		}

		template<typename ... Visitor>
		auto visit(Visitor ... visitor) const {
			return std::visit(std_fixes::OverloadedVisitor{ visitor... }, value);
		}

		template<typename T>
		[[nodiscard]]
		T* getIf() {
			auto ptr = std::get_if<T>(&value);
			if (ptr == nullptr) {
				return {};
			}
			return ptr;
		}

		template<typename T>
		[[nodiscard]]
		const T* getIf() const {
			auto ptr = std::get_if<T>(&value);
			if (ptr == nullptr) {
				return {};
			}
			return ptr;
		}
	};
}
