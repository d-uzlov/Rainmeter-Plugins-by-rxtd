// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

#include "SyntaxTree.h"

namespace rxtd::expression_parser {
	class ASTSolver {
	public:
		using NodeData = GrammarDescription::NodeData;

	private:
		SyntaxTree tree;
		mutable std::vector<std::optional<NodeData>> values;

	public:
		ASTSolver();

		ASTSolver(SyntaxTree&& tree);

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
		/// When value is not known, implementation must return std::nullopt.
		/// Implementation is allowed to throw ASTSolver::ValueProvider::Exception from all functions.
		/// </summary>
		class ValueProvider {
		public:
			using NodeData = NodeData;

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

			virtual std::optional<NodeData> solveFunction(sview word, array_view<NodeData> values) {
				return {};
			}

			virtual std::optional<NodeData> solveWord(sview word) {
				return {};
			}

			virtual std::optional<NodeData> solveCustom(const std::any& value) {
				return {};
			}
		};

		/// <summary>
		/// Checks if all ternary operator values match each other.
		/// For the main ternaty operator ?: use part1=L"?" and part2=L":"
		/// Throws ASTSolver::Exception when operators don't match
		/// </summary>
		void checkTernaryOperator(sview part1 = L"?", sview part2 = L":");

		[[nodiscard]]
		const SyntaxTree& peekTree() const {
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
		void collapseNodes(ValueProvider& valueProvider);

	private:
		/// <summary>
		/// Recursively copy alive part of the tree (presented by @code oldNodes), descending from oldNodeIndex.
		/// </summary>
		/// <param name="oldNodeIndex">Index of the root node of the subtree</param>
		/// <param name="oldNodes">Nodes of the old syntax tree</param>
		/// <param name="valueProvider"></param>
		/// <returns>Index of new node in the tree</returns>
		[[nodiscard]]
		ast_nodes::IndexType copySubTree(ast_nodes::IndexType oldNodeIndex, array_span<GenericNode> oldNodes, ValueProvider& valueProvider);
	};
}
