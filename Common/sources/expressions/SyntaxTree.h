/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <utility>

#include "GrammarDescription.h"

namespace rxtd::common::expressions::ast_nodes {
	using IndexType = int32_t;

	struct OperatorNodeBase {
		sview operatorValue;
		OperatorInfo::OperatorFunction function = nullptr;

		OperatorNodeBase() = default;

		OperatorNodeBase(sview operatorValue, OperatorInfo::OperatorFunction function) : operatorValue(operatorValue), function(function) {}
	};

	struct NumberNode {
		double value;

		NumberNode() = default;

		explicit NumberNode(double value) : value(value) {}
	};

	struct BinaryOperatorNode : public OperatorNodeBase {
		IndexType left = 0;
		IndexType right = 0;

		BinaryOperatorNode() = default;

		BinaryOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType left, IndexType right) :
			OperatorNodeBase(operatorValue, function),
			left(left), right(right) {}
	};

	struct PrefixOperatorNode : public OperatorNodeBase {
		IndexType child = 0;

		PrefixOperatorNode() = default;

		PrefixOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType child) :
			OperatorNodeBase(operatorValue, function), child(child) {}
	};

	struct PostfixOperatorNode : public OperatorNodeBase {
		IndexType child = 0;

		PostfixOperatorNode() = default;

		PostfixOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType child) :
			OperatorNodeBase(operatorValue, function), child(child) {}
	};

	struct WordNode {
		sview word;

		WordNode() = default;

		WordNode(sview word) : word(word) {}
	};

	struct FunctionNode {
		sview word;
		std::vector<IndexType> children;

		FunctionNode() = default;

		FunctionNode(sview word, std::vector<IndexType> children) : word(word), children(std::move(children)) {}
	};

	struct CustomTerminalNode {
		std::any value;

		CustomTerminalNode() = default;

		CustomTerminalNode(std::any value) : value(std::move(value)) {}
	};

	struct GenericNode {
		std::variant<
			NumberNode,
			BinaryOperatorNode,
			PrefixOperatorNode,
			PostfixOperatorNode,
			WordNode,
			FunctionNode,
			CustomTerminalNode
		> value{};

		GenericNode() = default;

		template<typename T>
		GenericNode(T t) : value(std::move(t)) { }

		template<typename Visitor>
		auto visit(Visitor visitor) const {
			return std::visit(visitor, value);
		}
	};

	class SyntaxTree {
		std::vector<GenericNode> nodes;

	public:
		[[nodiscard]]
		array_view<GenericNode> getNodes() const {
			return nodes;
		}

		[[nodiscard]]
		IndexType getRootIndex() const {
			return IndexType(nodes.size()) - 1;
		}

		void clear() {
			nodes.clear();
		}

		IndexType allocateNumber(double value) {
			nodes.emplace_back(NumberNode{ value });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocatePrefix(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType node) {
			nodes.emplace_back(PrefixOperatorNode{ operatorValue, function, node });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocatePostfix(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType node) {
			nodes.emplace_back(PostfixOperatorNode{ operatorValue, function, node });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocateBinary(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType left, IndexType right) {
			nodes.emplace_back(BinaryOperatorNode{ operatorValue, function, left, right });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocateFunction(sview word, std::vector<IndexType> children) {
			nodes.emplace_back(FunctionNode{ word, std::move(children) });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocateCustom(std::any value) {
			nodes.emplace_back(CustomTerminalNode{ std::move(value) });
			return IndexType(nodes.size()) - 1;
		}

		IndexType allocateWord(sview value) {
			nodes.emplace_back(WordNode{ value });
			return IndexType(nodes.size()) - 1;
		}
	};
}
