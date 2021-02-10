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

		ast_nodes::SyntaxTree tree;
		mutable std::vector<std::optional<double>> values;

	public:
		ASTSolver();

		ASTSolver(ast_nodes::SyntaxTree&& tree);

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
		void checkTernaryOperator();

		[[nodiscard]]
		const ast_nodes::SyntaxTree& peekTree() const {
			return tree;
		}

		/// <summary>
		/// Collapses all the constant subtrees into numbers.
		///
		/// This function can throw ASTSolver::Exception when old tree is invalid.
		/// This function can pass ASTSolver::ValueProvider::Exception from ValueProvider.
		/// </summary>
		void optimize(ValueProvider* valueProvider);

		/// <summary>
		/// Calculates result of expression.
		/// If the expression is not a constant, throws exception.
		///
		/// This function can throw ASTSolver::Exception when old tree is invalid.
		/// This function can pass ASTSolver::ValueProvider::Exception from ValueProvider.
		/// </summary>
		/// <returns>
		/// Result of the expression.
		/// </returns>
		[[nodiscard]]
		double solve(ValueProvider* valueProvider) const;

		/// <summary>
		/// Calculates values for all constant nodes
		///
		/// This function can throw ASTSolver::Exception when old tree is invalid.
		/// This function can pass ASTSolver::ValueProvider::Exception from ValueProvider.
		/// </summary>
		void collapseNodes(ValueProvider* valueProvider);

	private:
		/// <summary>
		/// Recursively copy alive part of the tree (presented by @code oldNodes), descending from oldNodeIndex.
		/// </summary>
		/// <param name="oldNodeIndex">Index of the root node of the subtree</param>
		/// <param name="oldNodes">Nodes of the old syntax tree</param>
		/// <returns>Index of new node in the tree</returns>
		[[nodiscard]]
		ast_nodes::IndexType copySubTree(ast_nodes::IndexType oldNodeIndex, array_view<ast_nodes::GenericNode> oldNodes);
	};
}
