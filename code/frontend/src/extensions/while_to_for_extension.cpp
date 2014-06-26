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

#include <iomanip>
#include <ostream>
#include <string>

#include "insieme/frontend/extensions/while_to_for_extension.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/ir++_utils.h"
#include "insieme/core/ir.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/pattern/ir_pattern.h"
#include "insieme/core/pattern/pattern_utils.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/utils/assert.h"

namespace pattern = insieme::core::pattern;
namespace irp = insieme::core::pattern::irp;
namespace printer = insieme::core::printer;

namespace insieme {
namespace frontend {

namespace {
	printer::PrettyPrinter pp(const core::NodePtr& n) {
		return printer::PrettyPrinter(n, printer::PrettyPrinter::NO_LET_BINDINGS);
	}
}

/// Determines the maximum node path depth given node n as the root node. This procedure is most likely be
/// used before or during a PrintNodes invocation.
unsigned int MaxDepth(const insieme::core::Address<const insieme::core::Node> n) {
	unsigned max = 0;
	for (auto c: n.getChildAddresses()) {
		unsigned now=MaxDepth(c);
		if (now>max) max=now;
	}
	return max+1;
}

/// Print the nodes to std::cout starting from root n, one by one, displaying the node path
/// and the visual representation. The output can be prefixed by a string, and for visually unifying several
/// PrintNodes invocations the maximum (see MaxDepth) and current depth may also be given.
void PrintNodes(const insieme::core::Address<const insieme::core::Node> n, std::string prefix="        @\t",
				unsigned int max=0, unsigned int depth=0) {
	if (!max) max=MaxDepth(n);
	for (auto c: n.getChildAddresses()) {
		std::cout << prefix << c << std::setw(2*(max-depth-1)) << "+"
				  << std::setw(2*depth+1) << "" << *c << std::endl;
		PrintNodes(c, prefix, max, depth+1);
	}
}

/// Given a node a, verify that it is a self-assignment to the variable with an added constant value,
/// and then extract the integer value (the step size in a for loop), returning it. As an example,
/// given the assignment "x = x - 5", this function would return the integer "-5".
int extractStepFromAssignment(core::Address<const core::Node> a) {
	std::cout << std::endl << "assignment: " << pp(a) << std::endl;

	// set up the patterns and do the matching
	auto operatorpat=pattern::single(irp::literal("int.sub")|irp::literal("int.add"));
	auto assignpat=irp::assignment
			(pattern::var("lhs"), pattern::node(
				 pattern::any									// any type will do (could be integer)
				 << pattern::listVar("addsub", operatorpat)		// operation plus or minus, determines step
				 << pattern::listVar("ops", *pattern::any)));	// operation arguments
	pattern::AddressMatchOpt m=assignpat->matchAddress(core::NodeAddress(a));

	// check whether we have a match, and can assign the nodes to the variables for further investigation
	// consider for now only IRs in the form: v1 = v1 +/- ...
	core::Address<const core::Node> lhs, addsub, op1, op2;
	bool ok=false;
	if (m && m.get().isVarBound("lhs") && m.get().isVarBound("addsub") && m.get().isVarBound("ops")) {
		std::vector<core::Address<const core::Node> >
				addsubvec=m.get()["addsub"].getFlattened(),
				opsvec=m.get()["ops"].getFlattened();
		if (addsubvec.size()==1 && opsvec.size()==2) {
			lhs=m.get()["lhs"].getValue();
			addsub=addsubvec[0];
			op1=opsvec[0];
			op2=opsvec[1];
			ok=true;
			// debug output: what are we processing? did the pattern matching work? don't fly blind!
			/*std::cout << "\t# lhs(" << lhs << ")=" << *lhs
					<< " addsub(" << addsub << ")=" << *addsub
					<< " op1(" << op1 << ")=" << *op1
					<< " op2(" << op2 << ")=" << *op2 << std::endl;*/
		}
	}

	// now we need to set up some IR to compare our nodes against
	core::Address<core::Literal>   intsub, intadd;
	core::Address<core::Statement> stmt;
	core::Pointer<const core::Node> op2ptr=op2.getAddressedNode();
	if (ok) {
		// get the node manager and create int.sub, int.add literals for comparison
		core::NodeManager& mgr=addsub->getNodeManager();
		auto& basic=mgr.getLangBasic();
		intsub=core::LiteralAddress(basic.getSignedIntSub());
		intadd=core::LiteralAddress(basic.getSignedIntAdd());

		// create an IR builder and create an expression from the LHS variable to compare to
		core::IRBuilder builder(mgr);
		stmt=core::Address<core::Statement>(builder.deref(lhs.as<core::ExpressionPtr>()));
		ok=*stmt==*op1; // our first argument should be something like int.add(int<4> ref.deref v1)
		ok&=op2ptr.getNodeType()==core::NT_Literal; // our second argument should be a literal
	}

	// return step size = 0 in case our node did not satisfy our requirements
	int step=0;
	if (ok) {
		step=(*addsub==*intsub?-1:1)*op2ptr.as<core::Pointer<const core::Literal> >().getValueAs<int>();
		//std::cout << "\tstep size: " << step << std::endl;
	}

	return step;
}

/// while statements can be for statements iff only one variable used in the condition is
/// altered within the statement, and this alteration satisfies certain conditions
insieme::core::ProgramPtr WhileToForPlugin::IRVisit(insieme::core::ProgramPtr& prog) {
		auto whilepat = irp::whileStmt(pattern::var("condition", pattern::all(pattern::var("cvar", irp::variable()))),
									   pattern::var("body"));

		irp::replaceAll(whilepat, prog, [&](pattern::AddressMatch match) {
			auto condition = match["condition"].getValue();
			auto body = match["body"].getValue();
			unsigned int varcount=0, maxassign=0;

			std::cout << std::endl
					  << "while-to-for Transformation (condition " << pp(condition) << "):" << std::endl
					  << pp(match.getRoot()) << std::endl << std::endl;

			// collect all variables from the loop condition, and store them in a PointerSet
			insieme::utils::set::PointerSet<core::VariablePtr> cvarSet;
			for(auto cvar: match["cvar"].getFlattened()) {
				auto varptr=cvar.getAddressedNode().as<core::VariablePtr>();
				if (!cvarSet.contains(varptr)) cvarSet.insert(varptr);
			}
			//std::cout << "condition variables: " << cvarSet << std::endl << std::endl;

			// for each condition variable, find its assignments in the loop body
			// do not consider the rhs here, as the rhs not necessarily includes the variable in question and/or
			// an addition/subtraction expression
			std::vector<insieme::core::Address<const insieme::core::Node> > assignments;
			for(core::VariablePtr var: cvarSet) {
				// do pattern matching for one variable
				auto assignpat=irp::assignment(irp::atom(var), pattern::any);
				auto assignall=pattern::aT(pattern::all(pattern::var("assignment", assignpat)));
				pattern::AddressMatchOpt nodes=assignall->matchAddress(core::NodeAddress(body.getAddressedNode()));

				// if the variable matched the pattern in the loop body, save the assignment
				if (nodes && nodes.get().isVarBound("assignment")) {
					auto myassignments=nodes.get()["assignment"].getFlattened();
					assignments.insert(assignments.end(), myassignments.begin(), myassignments.end());
					++varcount;
					if (myassignments.size()>maxassign) maxassign=myassignments.size();
				}
			}

			// TODO
			for (auto a: assignments) {
				int step=extractStepFromAssignment(a);

				std::vector<insieme::core::Address<const insieme::core::Node> > blacklist;
				if (step) {
					std::cout << "step size is " << step << std::endl;
				} else {
					std::cout << "loop is no for loop!" << std::endl;
					blacklist.push_back(a);
				}
			}

			// debug information: print the modified loop
			std::cout << std::endl << varcount << " vars encountered, maximum "
					  << maxassign << " assignments" << std::endl
					  << "Loop is now:\n" << pp(match.getRoot()) << std::endl << std::endl;
			return match.getRoot().getAddressedNode();
		} );
		
		return prog;
}

} // frontend
} // insieme
