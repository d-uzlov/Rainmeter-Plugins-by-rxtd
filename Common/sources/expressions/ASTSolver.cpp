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


void ASTSolver::checkTernaryOperator(const ast_nodes::SyntaxTree& tree) {
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

std::variant<double, ast_nodes::SyntaxTree> ASTSolver::optimize(const ast_nodes::SyntaxTree& oldTree, ValueProvider* valueProvider) {
	ASTSolver solver{ oldTree.getNodes() };

	solver.trySolve(valueProvider);

	auto root = solver.nodesEvaluated.back();

	if (root.has_value()) {
		return root.value().value;
	}

	solver.copySubTree(oldTree.getRootIndex());

	return std::move(solver.newTree);
}

void ASTSolver::trySolve(ValueProvider* valueProvider) {
	nodesEvaluated.clear();
	nodesEvaluated.resize(oldNodes.size());

	for (index i = 0; i < oldNodes.size(); i++) {
		const auto& genericNode = oldNodes[i];

		std::visit(
			overloaded{
				[&](const ast_nodes::NumberNode& node) { nodesEvaluated[i] = Data{ node.value }; },
				[&](const ast_nodes::PrefixOperatorNode& node) {
					if (nodesEvaluated[node.child].has_value()) {
						nodesEvaluated[i] = node.function(nodesEvaluated[node.child].value(), {});
					}
				},
				[&](const ast_nodes::PostfixOperatorNode& node) {
					if (nodesEvaluated[node.child].has_value()) {
						nodesEvaluated[i] = node.function(nodesEvaluated[node.child].value(), {});
					}
				},
				[&](const ast_nodes::BinaryOperatorNode& node) {
					if (nodesEvaluated[node.left].has_value() && nodesEvaluated[node.right].has_value()) {
						nodesEvaluated[i] = node.function(nodesEvaluated[node.left].value(), nodesEvaluated[node.right].value());
					}
				},
				[&](const ast_nodes::CustomTerminalNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					auto valueOpt = valueProvider->solveCustom(node.value);
					if (valueOpt.has_value()) {
						nodesEvaluated[i] = Data{ valueOpt.value() };
					}
				},
				[&](const ast_nodes::WordNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					auto valueOpt = valueProvider->solveWord(node.word);
					if (valueOpt.has_value()) {
						nodesEvaluated[i] = Data{ valueOpt.value() };
					}
				},
				[&](const ast_nodes::FunctionNode& node) {
					if (valueProvider == nullptr) {
						return;
					}

					bool allEvaluated = true;
					std::vector<double> values;

					for (auto child : node.children) {
						if (nodesEvaluated[child].has_value()) {
							values.push_back(nodesEvaluated[child].value().value);
						} else {
							allEvaluated = false;
							break;
						}
					}

					if (allEvaluated) {
						auto valueOpt = valueProvider->solveFunction(node.word, values);
						if (valueOpt.has_value()) {
							nodesEvaluated[i] = Data{ valueOpt.value() };
						}
					}
				}
			}, genericNode.value
		);
	}

}

ast_nodes::IndexType ASTSolver::copySubTree(ast_nodes::IndexType oldNodeIndex) {
	auto genericNode = oldNodes[oldNodeIndex];

	ast_nodes::IndexType result{};

	std::visit(
		overloaded{
			[&](const auto&) {
				throw Exception{ L"Unexpected node type" };
			},
			[&](const ast_nodes::PrefixOperatorNode& node) {
				const auto child = nodesEvaluated[node.child].has_value()
				                   ? newTree.allocateNumber(nodesEvaluated[node.child].value().value)
				                   : copySubTree(node.child);

				result = newTree.allocatePrefix(node.operatorValue, node.function, child);
			},
			[&](const ast_nodes::PostfixOperatorNode& node) {
				const auto child = nodesEvaluated[node.child].has_value()
				                   ? newTree.allocateNumber(nodesEvaluated[node.child].value().value)
				                   : copySubTree(node.child);

				result = newTree.allocatePostfix(node.operatorValue, node.function, child);
			},
			[&](const ast_nodes::BinaryOperatorNode& node) {
				const auto left = nodesEvaluated[node.left].has_value()
				                  ? newTree.allocateNumber(nodesEvaluated[node.left].value().value)
				                  : copySubTree(node.left);

				const auto right = nodesEvaluated[node.right].has_value()
				                   ? newTree.allocateNumber(nodesEvaluated[node.right].value().value)
				                   : copySubTree(node.right);

				result = newTree.allocateBinary(node.operatorValue, node.function, left, right);
			},
			[&](const ast_nodes::CustomTerminalNode& node) {
				result = newTree.allocateCustom(node.value);
			},
			[&](const ast_nodes::WordNode& node) {
				result = newTree.allocateWord(node.word);
			},
			[&](const ast_nodes::FunctionNode& node) {
				std::vector<ast_nodes::IndexType> children;

				for (auto oldChild : node.children) {
					auto child = nodesEvaluated[oldChild].has_value()
					             ? newTree.allocateNumber(nodesEvaluated[oldChild].value().value)
					             : copySubTree(oldChild);
					children.emplace_back(child);
				}

				result = newTree.allocateFunction(node.word, std::move(children));
			}
		}, genericNode.value
	);

	return result;
}
