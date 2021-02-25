/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ASTSolver.h"

using rxtd::expression_parser::ASTSolver;
using rxtd::expression_parser::ast_nodes::IndexType;

ASTSolver::ASTSolver() {
	tree.allocateNode<ast_nodes::ConstantNode>(0.0);
}

ASTSolver::ASTSolver(SyntaxTree&& tree): tree(std::move(tree)) {}

void ASTSolver::checkTernaryOperator(sview part1, sview part2) {
	auto nodes = tree.getNodes();

	std::set<index> qMarkPlaces;
	for (index i = 0; i < nodes.size(); i++) {
		auto* binaryNode = nodes[i].getIf<ast_nodes::BinaryOperatorNode>();
		if (binaryNode != nullptr) {
			if (binaryNode->opInfo.operatorValue == part1) {
				qMarkPlaces.insert(i);
			}

			if (binaryNode->opInfo.operatorValue == part2) {
				auto* left = nodes[binaryNode->left].getIf<ast_nodes::BinaryOperatorNode>();
				if (left == nullptr || left->opInfo.operatorValue != part1) {
					throw Exception{ L"Ternary operator symbols don't match" };
				}
				qMarkPlaces.erase(binaryNode->left);
			}
		}
	}

	if (!qMarkPlaces.empty()) {
		throw Exception{ L"Ternary operator symbols don't match" };
	}
}

void ASTSolver::optimize(ValueProvider* valueProvider) {
	ValueProvider defaultValueProvider;
	if (valueProvider == nullptr) {
		valueProvider = &defaultValueProvider;
	}

	collapseNodes(*valueProvider);

	SyntaxTree oldTree = std::exchange(tree, {});
	(void)copySubTree(oldTree.getRootIndex(), oldTree.getNodes(), *valueProvider);

	tree.setSource(std::move(oldTree).consumeSource());
}

double ASTSolver::solve(ValueProvider* valueProvider) const {
	using ast_nodes::ConstantNode;

	auto nodes = tree.getNodes();

	values.clear();
	values.resize(nodes.size());

	for (index i = 0; i < nodes.size(); i++) {
		values[i] = nodes[i].visit(
			[](const auto&) -> NodeData { throw Exception{ L"Unexpected node type" }; },
			[](const ConstantNode& node) { return node.data; },
			[&](const ast_nodes::PrefixOperatorNode& node) {
				auto child = values[node.child];
				if (!child.has_value()) throw Exception{ L"Expression is not a constant" };
				return node.opInfo.solveFunction(NodeData{ child.value() }, {});
			},
			[&](const ast_nodes::PostfixOperatorNode& node) {
				auto child = values[node.child];
				if (!child.has_value()) throw Exception{ L"Expression is not a constant" };
				return node.opInfo.solveFunction(NodeData{ child.value() }, {});
			},
			[&](const ast_nodes::BinaryOperatorNode& node) {
				auto left = values[node.left];
				auto right = values[node.right];
				if (!left.has_value() || !right.has_value()) throw Exception{ L"Expression is not a constant" };
				return node.opInfo.solveFunction(NodeData{ left.value() }, NodeData{ right.value() });
			},
			[&](const ast_nodes::CustomTerminalNode& node) {
				if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

				auto valueOpt = valueProvider->solveCustom(node.value);
				if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
				return valueOpt.value();
			},
			[&](const ast_nodes::WordNode& node) {
				if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

				auto valueOpt = valueProvider->solveWord(node.word);
				if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
				return valueOpt.value();
			},
			[&](const ast_nodes::FunctionNode& node) {
				if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

				std::vector<NodeData> args;

				for (auto childIndex : node.children) {
					auto child = values[childIndex];
					if (child.has_value()) throw Exception{ L"Expression is not a constant" };
					args.push_back(child.value());
				}

				auto valueOpt = valueProvider->solveFunction(node.word, args);
				if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
				return valueOpt.value();
			}
		);
	}

	return values.back().value().getValue();
}

void ASTSolver::collapseNodes(ValueProvider& valueProvider) {
	using ast_nodes::ConstantNode;

	auto nodes = tree.getNodes();
	for (auto& genericNode : nodes) {
		genericNode.visit(
			[&](const auto&) { throw Exception{ L"Unexpected node type" }; },
			[&](const ConstantNode& node) { /* do nothing */ },
			[&](const ast_nodes::PrefixOperatorNode& node) {
				auto c1 = nodes[node.child].getIf<ConstantNode>();
				if (c1 != nullptr) {
					genericNode = ConstantNode{ node.opInfo.solveFunction(c1->data, {}) };
				}
			},
			[&](const ast_nodes::PostfixOperatorNode& node) {
				auto c1 = nodes[node.child].getIf<ConstantNode>();
				if (c1 != nullptr) {
					genericNode = ConstantNode{ node.opInfo.solveFunction(c1->data, {}) };
				}
			},
			[&](const ast_nodes::BinaryOperatorNode& node) {
				auto c1 = nodes[node.left].getIf<ConstantNode>();
				auto c2 = nodes[node.right].getIf<ConstantNode>();
				if (c1 != nullptr) {
					genericNode = ConstantNode{ node.opInfo.solveFunction(c1->data, c2->data) };
				}
			},
			[&](const ast_nodes::CustomTerminalNode& node) {
				auto valueOpt = valueProvider.solveCustom(node.value);
				if (valueOpt.has_value()) {
					genericNode = ConstantNode{ valueOpt.value() };
				}
			},
			[&](const ast_nodes::WordNode& node) {
				auto valueOpt = valueProvider.solveWord(node.word);
				if (valueOpt.has_value()) {
					genericNode = ConstantNode{ valueOpt.value() };
				}
			},
			[&](const ast_nodes::FunctionNode& node) {
				bool allEvaluated = true;
				std::vector<NodeData> values;

				for (auto child : node.children) {
					auto childPtr = nodes[child].getIf<ConstantNode>();
					if (childPtr != nullptr) {
						values.push_back(childPtr->data);
					} else {
						allEvaluated = false;
						break;
					}
				}

				if (allEvaluated) {
					auto valueOpt = valueProvider.solveFunction(node.word, values);
					if (valueOpt.has_value()) {
						genericNode = ConstantNode{ valueOpt.value() };
					}
				}
			}
		);
	}
}

IndexType ASTSolver::copySubTree(IndexType oldNodeIndex, array_span<GenericNode> oldNodes, ValueProvider& valueProvider) {
	return oldNodes[oldNodeIndex].visit(
		[&](auto&) -> IndexType {
			throw Exception{ L"Unexpected node type" };
		},
		[&](ast_nodes::ConstantNode& node) {
			return tree.allocateNode<ast_nodes::ConstantNode>(node.data);
		},
		[&](ast_nodes::PrefixOperatorNode& node) {
			const auto child = copySubTree(node.child, oldNodes, valueProvider);
			return tree.allocateNode<ast_nodes::PrefixOperatorNode>(node.opInfo, child);
		},
		[&](ast_nodes::PostfixOperatorNode& node) {
			const auto child = copySubTree(node.child, oldNodes, valueProvider);
			return tree.allocateNode<ast_nodes::PostfixOperatorNode>(node.opInfo, child);
		},
		[&](ast_nodes::BinaryOperatorNode& node) {
			const auto left = copySubTree(node.left, oldNodes, valueProvider);
			const auto right = copySubTree(node.right, oldNodes, valueProvider);
			return tree.allocateNode<ast_nodes::BinaryOperatorNode>(node.opInfo, left, right);
		},
		[&](ast_nodes::CustomTerminalNode& node) {
			return tree.allocateNode<ast_nodes::CustomTerminalNode>(std::move(node.value));
		},
		[&](ast_nodes::WordNode& node) {
			return tree.allocateNode<ast_nodes::WordNode>(node.word);
		},
		[&](ast_nodes::FunctionNode& node) {
			for (auto& child : node.children) {
				child = copySubTree(child, oldNodes, valueProvider);
			}

			return tree.allocateNode<ast_nodes::FunctionNode>(node.word, std::move(node.children));
		}
	);
}
