/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once


#include "GrammarDescription.h"

namespace rxtd::common::expressions::ast_nodes {
	using IndexType = int32_t;

	struct OperatorNodeBase {
		sview operatorValue{};
		OperatorInfo::OperatorFunction function{};

		OperatorNodeBase() = default;

		OperatorNodeBase(sview operatorValue, OperatorInfo::OperatorFunction function) : operatorValue(operatorValue), function(function) {}
	};

	struct NumberNode {
		double value{};

		NumberNode() = default;

		explicit NumberNode(double value) : value(value) {}
	};

	struct BinaryOperatorNode : public OperatorNodeBase {
		IndexType left{};
		IndexType right{};

		BinaryOperatorNode() = default;

		BinaryOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType left, IndexType right) :
			OperatorNodeBase(operatorValue, function),
			left(left), right(right) {}
	};

	struct PrefixOperatorNode : public OperatorNodeBase {
		IndexType child{};

		PrefixOperatorNode() = default;

		PrefixOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType child) :
			OperatorNodeBase(operatorValue, function), child(child) {}
	};

	struct PostfixOperatorNode : public OperatorNodeBase {
		IndexType child{};

		PostfixOperatorNode() = default;

		PostfixOperatorNode(sview operatorValue, OperatorInfo::OperatorFunction function, IndexType child) :
			OperatorNodeBase(operatorValue, function), child(child) {}
	};

	struct WordNode {
		sview word{};

		WordNode() = default;

		WordNode(sview word) : word(word) {}
	};

	struct FunctionNode {
		sview word{};
		std::vector<IndexType> children{};

		FunctionNode() = default;

		FunctionNode(sview word, std::vector<IndexType> children) : word(word), children(std::move(children)) {}
	};

	struct CustomTerminalNode {
		std::any value{};

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
		// This must be a vector and not a string
		// because small string optimization
		// kills string_views on the move.
		std::vector<wchar_t> source;
		std::vector<GenericNode> nodes;

	public:
		void setSource(std::vector<wchar_t> value) {
			source = std::move(value);
		}

		void setSource(sview value) {
			source.assign(value.begin(), value.end());
		}

		[[nodiscard]]
		sview getSource() const {
			return { source.data(), source.size() };
		}

		[[nodiscard]]
		std::vector<wchar_t> consumeSource() && {
			return std::move(source);
		}

		[[nodiscard]]
		array_span<GenericNode> getNodes() {
			return nodes;
		}

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

		template<typename Callable>
		void visitNodes(Callable callable) const {
			for (auto& node : nodes) {
				std::visit(callable, node.value);
			}
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
