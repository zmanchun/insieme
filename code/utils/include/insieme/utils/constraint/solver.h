/**
 * Copyright (c) 2002-2014 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details
 * regarding third party software licenses.
 */

#pragma once

#include <set>
#include <map>
#include <tuple>

#include <memory>
#include <type_traits>
#include <initializer_list>

#include "insieme/utils/printable.h"
#include "insieme/utils/set_utils.h"
#include "insieme/utils/map_utils.h"

#include "insieme/utils/assert.h"
#include "insieme/utils/constraint/assignment.h"
#include "insieme/utils/constraint/constraints.h"


namespace insieme {
namespace utils {
namespace constraint {


	// ----------------------------- Solver ------------------------------

	// The type of entities capable of resolving constraints.
	typedef std::function<Constraints(const std::set<Variable>&)> ConstraintResolver;

	class LazySolver {
		/**
		 * The source of lazy-generated constraints.
		 */
		ConstraintResolver resolver;

		/**
		 * The list of maintained constraints.
		 */
		Constraints constraints;

		/**
		 * The current partial solution.
		 */
		Assignment ass;

		/**
		 * The set of sets for which constraints have already been resolved.
		 */
		std::unordered_set<Variable> resolved;

		/**
		 * A lazily constructed graph of constraint dependencies.
		 */
		typedef std::unordered_map<Variable, std::set<const Constraint*>> Edges;
		Edges edges;

		/**
		 * A set of fully resolved constraints (all inputs resolved, just for performance)
		 */
		std::unordered_set<const Constraint*> resolvedConstraints;

	  public:
		LazySolver(const ConstraintResolver& resolver, const Assignment& initial = Assignment()) : resolver(resolver), ass(initial) {}

		/**
		 * Obtains an assignment including the solution of the requested set. This is an incremental
		 * approach and may be used multiple times. Previously computed results will be reused.
		 */
		const Assignment& solve(const Variable& set);

		/**
		 * Obtains an assignment including solutions for the given sets. This is an incremental
		 * approach and may be used multiple times. Previously computed results will be reused.
		 */
		const Assignment& solve(const std::set<Variable>& sets);

		/**
		 * Obtains a reference to the list of constraints maintained internally.
		 */
		const Constraints& getConstraints() const {
			return constraints;
		}

		/**
		 * Obtains all constraints for the given value set.
		 */
		const std::set<const Constraint*>& getConstraintsFor(const Variable& value) const {
			const static std::set<const Constraint*> empty;
			auto pos = edges.find(value);
			return (pos == edges.end()) ? empty : pos->second;
		}

		/**
		 * Obtains a reference to the current assignment maintained internally.
		 */
		const Assignment& getAssignment() const {
			return ass;
		}

		bool isResolved(const Variable& set) const {
			return resolved.find(set) != resolved.end();
		}

	  private:
		// -- internal utility functions ---

		bool hasUnresolvedInput(const Constraint& cur);

		void resolveConstraints(const Constraint& cur, vector<Variable>& worklist);

		void resolveConstraints(const std::vector<Variable>& values, vector<Variable>& worklist);
	};

	// an eager solver implementation
	Assignment solve(const Constraints& constraints, Assignment initial = Assignment());

	// a lazy solver for a single set
	Assignment solve(const Variable& set, const ConstraintResolver& resolver, Assignment initial = Assignment());

	// a lazy solver implementation
	Assignment solve(const std::set<Variable>& sets, const ConstraintResolver& resolver, Assignment initial = Assignment());

} // end namespace constraint
} // end namespace utils
} // end namespace insieme
