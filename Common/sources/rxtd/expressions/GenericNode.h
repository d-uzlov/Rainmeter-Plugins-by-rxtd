/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "OperatorNodes.h"

namespace rxtd::common::expressions {
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

	private:
		// from std::visit documentation
		template<class... Ts>
		struct overloadedVisitor : Ts... {
			using Ts::operator()...;
		};

		template<class... Ts>
		overloadedVisitor(Ts ...) -> overloadedVisitor<Ts...>;

	public:
		template<typename ... Visitor>
		auto visit(Visitor ... visitor) {
			return std::visit(overloadedVisitor{ visitor... }, value);
		}

		template<typename ... Visitor>
		auto visit(Visitor ... visitor) const {
			return std::visit(overloadedVisitor{ visitor... }, value);
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
