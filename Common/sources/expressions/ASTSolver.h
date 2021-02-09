/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "SyntaxTree.h"

namespace rxtd::common::expressions {
	class ASTSolver {
		using Data = OperatorInfo::Data;

		std::vector<std::optional<Data>> nodesEvaluated;
		array_view<ast_nodes::GenericNode> oldNodes;
		ast_nodes::SyntaxTree newTree;

		ASTSolver(array_view<ast_nodes::GenericNode> oldNodes) : oldNodes(oldNodes) {}

	public:
		class Exception : public std::runtime_error {
			sview message;
		public:
			explicit Exception(sview message) : runtime_error(""), message(message) {}

			[[nodiscard]]
			auto getMessage() const {
				return message;
			}
		};

		/// <summary>
		/// Solver for dynamic values.
		/// Implementation is allowed to throw ASTSolver::ValueProvider::Exception from all functions.
		/// </summary>
		class ValueProvider {
		public:
			class Exception : public ASTSolver::Exception {
				sview cause;

			public:
				explicit Exception(sview message, sview cause) : ASTSolver::Exception(message), cause(cause) {}

				[[nodiscard]]
				auto getCause() const {
					return cause;
				}
			};

			virtual ~ValueProvider() = default;

			virtual std::optional<double> solveFunction(sview word, array_view<double> values) {
				return {};
			}

			virtual std::optional<double> solveWord(sview word) {
				return {};
			}

			virtual std::optional<double> solveCustom(const std::any& value) {
				return {};
			}
		};

		/// <summary>
		/// Throws ASTSolver::Exception when '?' and ':' don't match
		/// </summary>
		static void checkTernaryOperator(const ast_nodes::SyntaxTree& tree);

		/// <summary>
		/// Collapses all the constant subtrees into numbers.
		/// If the whole tree is constant, returns the number it has evaluated to.
		/// Else makes new SyntaxTree that consists of nodes
		/// that are either numbers or non-constant subtrees.
		///
		/// This function can throw ASTSolver::Exception when old tree is invalid.
		/// This function can pass ASTSolver::ValueProvider::Exception from ValueProvider.
		/// </summary>
		/// <param name="tree">Old syntax tree, that needs to be optimized.</param>
		[[nodiscard]]
		static std::variant<double, ast_nodes::SyntaxTree>
		optimize(const ast_nodes::SyntaxTree& oldTree, ValueProvider* valueProvider);

	private:
		/// <summary>
		/// Calculates values for all constant nodes
		/// </summary>
		void trySolve(ValueProvider* valueProvider);

		/// <summary>
		/// Recursively copy alive part of the tree, descending from oldNodeIndex.
		/// </summary>
		/// <returns>Index of new node in the newTree</returns>
		ast_nodes::IndexType copySubTree(ast_nodes::IndexType oldNodeIndex);
	};
}
