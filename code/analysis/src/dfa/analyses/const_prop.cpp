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

#include "insieme/analysis/dfa/analyses/const_prop.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"

#include "insieme/utils/logging.h"

using namespace insieme::core;

namespace insieme { namespace analysis { namespace dfa { namespace analyses {

/**
 * ConstantPropagation Problem
 */
typename ConstantPropagation::Base::value_type 
ConstantPropagation::meet(const typename ConstantPropagation::Base::value_type& lhs, 
						  const typename ConstantPropagation::Base::value_type& rhs) const 
{
	typedef typename ConstantPropagation::Base::value_type element_type;
	typedef dfa::Value<LiteralPtr> ConstantType;

	std::cout << "Meet (" << lhs << ", " << rhs << ") -> " << std::flush;
	
	/** 
	 * Given 2 dataflow values associated to a variable, returns the new dataflow value 
	 * after the meet operator is applied according to the following table:
	 *
	 * ---------------------
	 * TOP ^ x      = x 
	 * TOP ^ TOP    = TOP
	 * x   ^ y      = BOTTOM
	 * x   ^ BOTTOM = BOTTOM
	 * x   ^ x      = x 
	 *----------------------
	 */
	auto eval = [](const ConstantType& lhs, const ConstantType& rhs) -> ConstantType {

		if (lhs == dfa::top) { return rhs; }
		if (rhs == dfa::top) { return lhs; }

		if (lhs == dfa::bottom || rhs == dfa::bottom) { 
			return dfa::bottom; 
		}

		if (lhs == rhs ) { return lhs; }

		return dfa::bottom;
	};

	element_type ret;
	element_type::const_iterator lhs_it = lhs.begin(), rhs_it = rhs.begin(), it, end;

	while(lhs_it != lhs.end() && rhs_it != rhs.end()) {
		LOG(INFO) << *lhs_it << " " << *rhs_it;
		if(std::get<0>(*lhs_it) == std::get<0>(*rhs_it)) {
			ret.insert( std::make_tuple(std::get<0>(*lhs_it), 
						eval(std::get<1>(*lhs_it), std::get<1>(*rhs_it))) 
					  );
			++lhs_it; ++rhs_it;
			continue;
		}
		if (*lhs_it < *rhs_it) { ret.insert( *(lhs_it++) ); continue; }
		if (*lhs_it > *rhs_it) { ret.insert( *(rhs_it++) ); continue; }
	}

	// Take care of the remaining elements which have to be written back to the result 
	std::tie(it,end) = lhs_it == lhs.end() ? 
						 std::make_tuple(rhs_it, rhs.end()) : 
						 std::make_tuple(lhs_it, lhs.end());

	while( it != end) { ret.insert( *(it++) ); }
	std::cout << ret << std::endl;

	return ret;
}


dfa::Value<LiteralPtr> lookup(const VariablePtr& var, const ConstantPropagation::value_type& in) {
	
	auto fit = std::find_if(in.begin(), in.end(), 
		[&](const typename ConstantPropagation::value_type::value_type& cur) {
			return std::get<0>(cur) == var;
		});

	assert (fit!=in.end());

	return std::get<1>(*fit);
}

dfa::Value<LiteralPtr> eval(const ExpressionPtr& lit, const ConstantPropagation::value_type& in) {

	using namespace arithmetic;

	Formula f = toFormula(lit);
	if (f.isConstant()) { return toIR(lit->getNodeManager(), f).as<LiteralPtr>(); }

	if (f.isPolynomial()) {
		// this is a polynomial, if the variables of the polynomial are all constants, then we can compute this value

		ValueReplacementMap replacements;
		for(const auto& value : f.extractValues()) {
			VariablePtr expr = static_cast<ExpressionPtr>(value).as<VariablePtr>();
			dfa::Value<LiteralPtr> lit = lookup(expr,in);
			if (lit.isBottom()) return dfa::bottom;
			if (lit.isTop()) return dfa::top;

			replacements.insert({ value, toFormula(lit.value()) });
		}

		// Replace values with the constant values 
		f = f.replace(replacements);

		assert(f.isConstant());
		return toIR(lit->getNodeManager(), f).as<LiteralPtr>();
	}

}


ConstantPropagation::value_type
ConstantPropagation::transfer_func(const ConstantPropagation::value_type& in, const cfg::BlockPtr& block) const {

	typename ConstantPropagation::value_type gen, kill;
	
	if (block->empty()) { return in; }
	assert(block->size() == 1);

	LOG(DEBUG) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
	LOG(DEBUG) << "~ Block " << block->getBlockID();
	LOG(DEBUG) << "~ IN: " << in;

	for_each(block->stmt_begin(), block->stmt_end(), 
			[&] (const cfg::Element& cur) {

		StatementPtr stmt = cur.getAnalysisStatement();

		auto handle_def = [&](const VariablePtr& var, const ExpressionPtr& init) { 
			
			dfa::Value<LiteralPtr> res = eval(init, in);
			LOG(DEBUG) << "VAR: " << var << " eval-> " << res;
			gen.insert( std::make_tuple(var, res) );

			// kill all declarations reaching this block 
			std::copy_if(in.begin(), in.end(), std::inserter(kill,kill.begin()), 
					[&](const typename ConstantPropagation::value_type::value_type& cur){
						return std::get<0>(cur) == var;
					} );

		};

		if (stmt->getNodeType() == NT_Literal) { return; }

		// assume scalar variables 
		if (DeclarationStmtPtr decl = dynamic_pointer_cast<const DeclarationStmt>(stmt)) {

			handle_def( decl->getVariable(), decl->getInitialization() );

		} else if (CallExprPtr call = dynamic_pointer_cast<const CallExpr>(stmt)) {

			if (core::analysis::isCallOf(call, call->getNodeManager().getLangBasic().getRefAssign()) ) { 
				handle_def( call->getArgument(0).as<VariablePtr>(), call->getArgument(1) );
			}

			// do nothing otherwise

		} else {
			LOG(WARNING) << stmt;
			assert(false && "Stmt not handled");
		}
	});

	LOG(DEBUG) << "~ KILL: " << kill;
	LOG(DEBUG) << "~ GEN:  " << gen;

	typename ConstantPropagation::value_type set_diff, ret;
	std::set_difference(in.begin(), in.end(), kill.begin(), kill.end(), std::inserter(set_diff, set_diff.begin()));
	std::set_union(set_diff.begin(), set_diff.end(), gen.begin(), gen.end(), std::inserter(ret, ret.begin()));

	LOG(DEBUG) << "~ RET: " << ret;
	return ret;
}

} } } } // end insieme::analysis::dfa::analyses namespace 
