// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

#include "GenericNode.h"
#include "OperatorNodes.h"

namespace rxtd::expression_parser {
	class SyntaxTree {
		using IndexType = ast_nodes::IndexType;

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
			return static_cast<IndexType>(nodes.size()) - 1;
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

		template<typename NodeType, typename ... Args>
		IndexType allocateNode(Args ... args) {
			nodes.push_back(NodeType{ std::move(args)... });
			return static_cast<IndexType>(nodes.size()) - 1;
		}
	};
}
