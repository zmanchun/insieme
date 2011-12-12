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

#include "insieme/core/analysis/ir_utils.h"

#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_visitor.h"

// WARNING: this file is only preliminary and might be heavily modified or moved ...


namespace insieme {
namespace core {
namespace analysis {


bool isCallOf(const CallExprPtr& candidate, const NodePtr& function) {

	// check for null
	if (!candidate) {
		return false;
	}

	// check invoked function
	return *(candidate->getFunctionExpr()) == *function;
}

bool isCallOf(const NodePtr& candidate, const NodePtr& function) {

	// check for null
	if (!candidate) {
		return false;
	}

	// check type of node => has to be a call expression
	if (candidate->getNodeType() != NT_CallExpr) {
		return false;
	}

	// check invoked function
	return isCallOf(static_pointer_cast<const CallExpr>(candidate), function);
}

bool isNoOp(const StatementPtr& candidate) {

	// check for null
	if (!candidate) {
		return false;
	}

	// check type of statement => must be a compound statement
	if (candidate->getNodeType() != NT_CompoundStmt) {
		return false;
	}

	// must be an empty compound statement
	return static_pointer_cast<const CompoundStmt>(candidate)->getStatements().empty();
}

bool isRefOf(const NodePtr& candidate, const NodePtr& type) {

	// check for null
	if (!candidate) {
		return false;
	}

	// check type of node
	if (candidate->getNodeType() != NT_RefType) {
		return false;
	}

	// check element type
	return *(static_pointer_cast<const RefType>(candidate)->getElementType()) == *type;
}

bool isRefOf(const NodePtr& candidate, const NodeType kind) {

	// check for null
	if (!candidate) {
		return false;
	}

	// check type of node
	if (candidate->getNodeType() != NT_RefType) {
		return false;
	}

	// check element type (kind)
	return static_pointer_cast<const RefType>(candidate)->getElementType()->getNodeType() == kind;
}

bool isTypeLiteralType(const GenericTypePtr& type) {
	// check family name as well as type and name of parameters
	return type->getName()->getValue() == "type"
			&& type->getTypeParameter().size() == static_cast<std::size_t>(1)
			&& type->getIntTypeParameter().empty();
}

bool isTypeLiteralType(const TypePtr& type) {

	// check node type
	if (type->getNodeType() != core::NT_GenericType) {
		return false;
	}

	// forward test
	return isTypeLiteralType(static_pointer_cast<const core::GenericType>(type));
}

bool isConstructorExpr(const NodePtr& node) {
	NodeType pnt = node->getNodeType();
	return pnt == NT_VectorExpr || pnt == NT_StructExpr || pnt == NT_UnionExpr || pnt == NT_TupleExpr || pnt == NT_JobExpr;
}

// ------ Free Variable Extraction ----------

namespace {

	/**
	 * Will certainly determine the declaration status of variables inside a block.
	 */
	struct LambdaDeltaVisitor : public IRVisitor<bool> {
		insieme::utils::set::PointerSet<VariablePtr> bound;
		insieme::utils::set::PointerSet<VariablePtr> free;

		// do not visit types
		LambdaDeltaVisitor() : IRVisitor<bool>(false) {}

		bool visitNode(const NodePtr& node) { return false; } // default behavior: continue visiting

		bool visitDeclarationStmt(const DeclarationStmtPtr &decl) {
			bound.insert(decl->getVariable());
			return false;
		}

		bool visitVariable(const VariablePtr& var) {
			if(bound.find(var) == bound.end()) free.insert(var);
			return false;
		}

		// due to the structure of the IR, nested lambdas can never reuse outer variables
		//  - also prevents variables in LamdaDefinition from being inadvertently captured
		bool visitLambdaExpr(const LambdaExprPtr&) {
			return true;
		}

		// for bind, just look at the variables being bound and ignore the call
		bool visitBindExpr(const BindExprPtr& bindExpr) {
			ExpressionList expressions = bindExpr->getBoundExpressions();
			for_each(expressions, [&](const ExpressionPtr e) { this->visit(e); } );
			return true;
		}
	};

}

VariableList getFreeVariables(const NodePtr& code) {
	LambdaDeltaVisitor ldv;
	visitDepthFirstOncePrunable(code, ldv);

	// convert result into list
	VariableList res(ldv.free.begin(), ldv.free.end());
	return res;
}

} // end namespace utils
} // end namespace core
} // end namespace insieme
