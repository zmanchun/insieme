/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
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

#include <memory>

#include "insieme/analysis/dfa/entity.h"
#include "insieme/analysis/dfa/value.h"
#include "insieme/analysis/dfa/lattice.h"

#include "insieme/core/ir_expressions.h"

namespace insieme {
namespace analysis {
namespace dfa {


template <class Impl, class E, template <class> class Cont>
class Problem {

	// Builds the lattice 
	void init() {
		lattice_ptr = std::make_shared<LowerSemilattice<container_type>>(
				container_type(extracted), 
					top(), 
					bottom(), 
					std::bind(
						std::mem_fn(&Impl::meet), 
						static_cast<const Impl&>(*this), 
						std::placeholders::_1, 
						std::placeholders::_2
					)
				);
	}

public:
	typedef typename container_type_traits<E>::type extract_type;

	typedef Cont<extract_type> container_type;

	typedef typename container_type::value_type value_type;

	Problem(const CFG& cfg) : extracted( extract(E(), cfg) ) { }

	void initialize() { init(); }

	virtual value_type top() const = 0;

	virtual value_type bottom() const = 0;

	virtual value_type meet(const value_type& lhs, const value_type& rhs) const = 0;

	const extract_type& getExtracted() const { return extracted; }

	const LowerSemilattice<container_type>& getLattice() const { 
		assert(lattice_ptr && "Dataflow Problem not correctly initialized");
		return *lattice_ptr; 
	}

protected:
	extract_type extracted;
	std::shared_ptr<LowerSemilattice<container_type>> lattice_ptr;
};




/**
 * Define the DataFlow problem for Live variables 
 *
 * The Dataflow problem is defined on the powerset of the variables contained in
 * the code segment. 
 *
 * The TOP element of the generated lattice is the set of variables, while the
 * BOTTOM is the empty set 
 *
 * The MEET operator is the intersection operation
 */
class LiveVariables: public Problem<LiveVariables, Entity<dfa::elem<core::VariablePtr>>, PowerSet> {

	typedef Problem<LiveVariables,Entity<dfa::elem<core::VariablePtr>>, PowerSet> Base;
	
public:

	typedef typename Base::value_type value_type;

	LiveVariables(const CFG& cfg): Base(cfg) { }

	virtual typename Base::value_type top() const { 
		// the top element is the set of all variable present in the program
		return extracted;
	}

	virtual typename Base::value_type bottom() const {
		// the bottom element is the empty set 
		return typename Base::value_type();
	}

	typename Base::value_type
		meet(const typename Base::value_type& lhs, const typename Base::value_type& rhs) const 
	{
		typedef typename Base::value_type ResultType;

		ResultType ret;
		std::set_intersection(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::inserter(ret,ret.begin()));
		return ret;
	}

};


/**
 * Define the DataFlowProblem for Constant Propagation
 */
class ConstantPropagation: 
	public Problem<ConstantPropagation, 
				   Entity<dfa::elem<core::VariablePtr>, dfa::dom<dfa::Value<core::LiteralPtr>>>,
				   PowerSet
		   > 
{

	typedef Problem<ConstantPropagation, 
				   Entity<dfa::elem<core::VariablePtr>, dfa::dom<dfa::Value<core::LiteralPtr>>>,
				   PowerSet
		   >  Base;
	
public:

	typedef typename Base::value_type value_type;

	ConstantPropagation(const CFG& cfg): Base(cfg) { }

	virtual typename Base::value_type top() const { 
		const auto& lhsBase = extracted.getLeftBaseSet();
		return makeCartProdSet(
				lhsBase, 
				std::set<dfa::Value<core::LiteralPtr>>( { dfa::Value<core::LiteralPtr>(dfa::top) } ) 
			).expand();
	}

	virtual typename Base::value_type bottom() const {
		const auto& lhsBase = extracted.getLeftBaseSet();
		return makeCartProdSet(
				lhsBase, 
				std::set<dfa::Value<core::LiteralPtr>>( { dfa::Value<core::LiteralPtr>(dfa::bottom) } ) 
			).expand();
	}

	typename Base::value_type meet(const typename Base::value_type& lhs, const typename Base::value_type& rhs) const;

};

} // end dfa namespace 
} // end analysis namespace 
} // end insieme namespace 
