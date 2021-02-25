/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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

		template<typename NodeType, typename ... Args>
		IndexType allocateNode(Args ... args) {
			nodes.push_back(NodeType{ std::move(args)... });
			return IndexType(nodes.size()) - 1;
		}
	};
}
