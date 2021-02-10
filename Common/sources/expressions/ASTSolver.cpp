/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ASTSolver.h"

using namespace common::expressions;

// from std::visit documentation
template<class... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts ...) -> overloaded<Ts...>;


ASTSolver::ASTSolver() {
	tree.allocateNumber(0.0);
	values.resize(tree.getNodes().size());
}

ASTSolver::ASTSolver(ast_nodes::SyntaxTree&& tree): tree(std::move(tree)) {
	values.resize(tree.getNodes().size());
}

void ASTSolver::checkTernaryOperator() {
	auto nodes = tree.getNodes();

	std::set<index> qMarkPlaces;
	for (index i = 0; i < nodes.size(); i++) {
		const auto& node = nodes[i];
		auto* binaryNode = std::get_if<ast_nodes::BinaryOperatorNode>(&node.value);
		if (binaryNode != nullptr) {
			if (binaryNode->operatorValue == L"?") {
				qMarkPlaces.insert(i);
			}

			if (binaryNode->operatorValue == L":") {
				auto* left = std::get_if<ast_nodes::BinaryOperatorNode>(&nodes[binaryNode->left].value);
				if (left == nullptr || left->operatorValue != L"?") {
					throw Exception{ L"Ternary operator ':' doesn't have a matching '?'" };
				}
				qMarkPlaces.erase(binaryNode->left);
			}
		}
	}

	if (!qMarkPlaces.empty()) {
		throw Exception{ L"Ternary operator '?' doesn't have a matching ':'" };
	}
}

void ASTSolver::optimize(ValueProvider* valueProvider) {
	collapseNodes(valueProvider);

	ast_nodes::SyntaxTree oldTree = std::exchange(tree, {});
	(void)copySubTree(oldTree.getRootIndex(), oldTree.getNodes());

	tree.setSource(std::move(oldTree).consumeSource());
}

double ASTSolver::solve(ValueProvider* valueProvider) const {
	using ast_nodes::NumberNode;
	using Data = OperatorInfo::Data;

	auto nodes = tree.getNodes();

	values.clear();
	values.resize(nodes.size());

	for (index i = 0; i < nodes.size(); i++) {
		auto& genericNode = nodes[i];
		std::visit(
			overloaded{
				[&](const auto&) { throw Exception{ L"Unexpected node type" }; },
				[&](const NumberNode& node) { values[i] = node.value; },
				[&](const ast_nodes::PrefixOperatorNode& node) {
					auto child = values[node.child];
					if (!child.has_value()) throw Exception{ L"Expression is not a constant" };
					values[i] = node.function(Data{ child.value() }, {}).value;
				},
				[&](const ast_nodes::PostfixOperatorNode& node) {
					auto child = values[node.child];
					if (!child.has_value()) throw Exception{ L"Expression is not a constant" };
					values[i] = node.function(Data{ child.value() }, {}).value;
				},
				[&](const ast_nodes::BinaryOperatorNode& node) {
					auto left = values[node.left];
					auto right = values[node.right];
					if (!left.has_value() || !right.has_value()) throw Exception{ L"Expression is not a constant" };
					values[i] = node.function(Data{ left.value() }, Data{ right.value() }).value;
				},
				[&](const ast_nodes::CustomTerminalNode& node) {
					if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

					auto valueOpt = valueProvider->solveCustom(node.value);
					if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
					values[i] = valueOpt.value();
				},
				[&](const ast_nodes::WordNode& node) {
					if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

					auto valueOpt = valueProvider->solveWord(node.word);
					if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
					values[i] = valueOpt.value();
				},
				[&](const ast_nodes::FunctionNode& node) {
					if (valueProvider == nullptr) throw Exception{ L"Expression is not a constant" };

					std::vector<double> args;

					for (auto childIndex : node.children) {
						auto child = values[childIndex];
						if (child.has_value()) throw Exception{ L"Expression is not a constant" };
						args.push_back(child.value());
					}

					auto valueOpt = valueProvider->solveFunction(node.word, args);
					if (!valueOpt.has_value()) throw Exception{ L"Expression is not a constant" };
					args[i] = valueOpt.value();
				}
			}, genericNode.value
		);
	}

	return values.back().value();
}

void ASTSolver::collapseNodes(ValueProvider* valueProvider) {
	using ast_nodes::NumberNode;
	using Data = OperatorInfo::Data;

	auto nodes = tree.getNodes();
	for (auto& genericNode : nodes) {
		std::visit(
			overloaded{
				[&](const auto&) {
					throw Exception{ L"Unexpected node type" };
				},
				[&](const NumberNode& node) { /* do nothing */ },
				[&](const ast_nodes::PrefixOperatorNode& node) {
					auto childPtr = std::get_if<NumberNode>(&nodes[node.child].value);
					if (childPtr != nullptr) {
						genericNode = NumberNode{ node.function(Data{ childPtr->value }, {}).value };
					}
				},
				[&](const ast_nodes::PostfixOperatorNode& node) {
					auto childPtr = std::get_if<NumberNode>(&nodes[node.child].value);
					if (childPtr != nullptr) {
						genericNode = NumberNode{ node.function(Data{ childPtr->value }, {}).value };
					}
				},
				[&](const ast_nodes::BinaryOperatorNode& node) {
					auto leftPtr = std::get_if<NumberNode>(&nodes[node.left].value);
					auto rightPtr = std::get_if<NumberNode>(&nodes[node.right].value);
					if (leftPtr != nullptr && rightPtr != nullptr) {
						genericNode = NumberNode{ node.function(Data{ leftPtr->value }, Data{ rightPtr->value }).value };
					}
				},
				[&](const ast_nodes::CustomTerminalNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					auto valueOpt = valueProvider->solveCustom(node.value);
					if (valueOpt.has_value()) {
						genericNode = NumberNode{ valueOpt.value() };
					}
				},
				[&](const ast_nodes::WordNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					auto valueOpt = valueProvider->solveWord(node.word);
					if (valueOpt.has_value()) {
						genericNode = NumberNode{ valueOpt.value() };
					}
				},
				[&](const ast_nodes::FunctionNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					bool allEvaluated = true;
					std::vector<double> values;

					for (auto child : node.children) {
						auto childPtr = std::get_if<NumberNode>(&nodes[child].value);
						if (childPtr != nullptr) {
							values.push_back(childPtr->value);
						} else {
							allEvaluated = false;
							break;
						}
					}

					if (allEvaluated) {
						auto valueOpt = valueProvider->solveFunction(node.word, values);
						if (valueOpt.has_value()) {
							genericNode = NumberNode{ valueOpt.value() };
						}
					}
				}
			}, genericNode.value
		);
	}
}

ast_nodes::IndexType ASTSolver::copySubTree(ast_nodes::IndexType oldNodeIndex, array_view<ast_nodes::GenericNode> oldNodes) {
	using ast_nodes::NumberNode;

	auto genericNode = oldNodes[oldNodeIndex];

	ast_nodes::IndexType result{};

	std::visit(
		overloaded{
			[&](const auto&) {
				throw Exception{ L"Unexpected node type" };
			},
			[&](const NumberNode& node) {
				result = tree.allocateNumber(node.value);
			},
			[&](const ast_nodes::PrefixOperatorNode& node) {
				auto childPtr = std::get_if<NumberNode>(&oldNodes[node.child].value);
				const auto child = copySubTree(node.child, oldNodes);
				result = tree.allocatePrefix(node.operatorValue, node.function, child);
			},
			[&](const ast_nodes::PostfixOperatorNode& node) {
				const auto child = copySubTree(node.child, oldNodes);
				result = tree.allocatePostfix(node.operatorValue, node.function, child);
			},
			[&](const ast_nodes::BinaryOperatorNode& node) {
				const auto left = copySubTree(node.left, oldNodes);
				const auto right = copySubTree(node.right, oldNodes);
				result = tree.allocateBinary(node.operatorValue, node.function, left, right);
			},
			[&](const ast_nodes::CustomTerminalNode& node) {
				result = tree.allocateCustom(node.value);
			},
			[&](const ast_nodes::WordNode& node) {
				result = tree.allocateWord(node.word);
			},
			[&](const ast_nodes::FunctionNode& node) {
				std::vector<ast_nodes::IndexType> children;

				for (auto oldChild : node.children) {
					const auto child = copySubTree(oldChild, oldNodes);
					children.emplace_back(child);
				}

				result = tree.allocateFunction(node.word, std::move(children));
			}
		}, genericNode.value
	);

	return result;
}
