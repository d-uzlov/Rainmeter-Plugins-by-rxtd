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
	using OperatorInfo = GrammarDescription::OperatorInfo;
	using MainOperatorInfo = GrammarDescription::MainOperatorInfo;
	using NodeData = GrammarDescription::NodeData;

	struct ConstantNode {
		NodeData data{};

		ConstantNode() = default;

		explicit ConstantNode(double data) : data(data) {}
		
		explicit ConstantNode(NodeData data) : data(data) {}
	};

	struct BinaryOperatorNode {
		MainOperatorInfo opInfo{};
		IndexType left{};
		IndexType right{};

		BinaryOperatorNode() = default;

		BinaryOperatorNode(MainOperatorInfo opInfo, IndexType left, IndexType right) :
			opInfo(opInfo), left(left), right(right) {}
	};

	struct PrefixOperatorNode {
		MainOperatorInfo opInfo{};
		IndexType child{};

		PrefixOperatorNode() = default;

		PrefixOperatorNode(MainOperatorInfo opInfo, IndexType child) : opInfo(opInfo), child(child) {}
	};

	struct PostfixOperatorNode {
		MainOperatorInfo opInfo{};
		IndexType child{};

		PostfixOperatorNode() = default;

		PostfixOperatorNode(MainOperatorInfo opInfo, IndexType child) : opInfo(opInfo), child(child) {}
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

		FunctionNode(sview word, std::vector<IndexType> children = {}) : word(word), children(std::move(children)) {}
	};

	struct CustomTerminalNode {
		std::any value{};

		CustomTerminalNode() = default;

		CustomTerminalNode(std::any value) : value(std::move(value)) {}
	};
}
