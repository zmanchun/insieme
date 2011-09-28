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

#include "insieme/core/transform/utils/member_access_literal_updater.h"
#include "insieme/core/ast_node.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/ast_visitor.h"

namespace insieme {
namespace core {
namespace transform {
#define BASIC builder.getNodeManager().basic
namespace utils {




/**
 * Visitor which checks if the type literal argument of compostite and tuple calls are aligned witht the actual type of the struct/tuple.
 * If not the type literal is replaced with the appropriate one
 */
const NodePtr MemberAccessLiteralUpdater::resolveElement(const NodePtr& ptr) {
	// if we reach a type stop recursion
	if (ptr->getNodeCategory() == NC_Type || ptr->getNodeCategory() == NC_IntTypeParam) {
		return ptr;
	}


	// recursive replacement has to be continued
	NodePtr res = ptr->substitute(builder.getNodeManager(), *this);

	if(const CallExprPtr& call = dynamic_pointer_cast<const CallExpr>(res)) {
		ExpressionPtr fun = call->getFunctionExpr();
		// struct access
		if(BASIC.isCompositeMemberAccess(fun)) {
			const StructTypePtr& structTy = static_pointer_cast<const StructType>(call->getArgument(0)->getType());
			// TODO find better way to extract Identifier from IdentifierLiteral
			const IdentifierPtr& id = builder.identifier(static_pointer_cast<const Literal>(call->getArgument(1))->getValue());
			const TypePtr& type = structTy->getTypeOfMember(id);
			if(call->getArgument(2)->getType() != type || call->getType() != type)
				res = builder.callExpr(type, fun, call->getArgument(0), call->getArgument(1), BASIC.getTypeLiteral(type));
		}

		if(BASIC.isCompositeRefElem(fun)) {
			const StructTypePtr& structTy = static_pointer_cast<const StructType>(
					static_pointer_cast<const RefType>(call->getArgument(0)->getType())->getElementType());
			// TODO find better way to extract Identifier from IdentifierLiteral
			const IdentifierPtr& id = builder.identifier(static_pointer_cast<const Literal>(call->getArgument(1))->getValue());
			const RefTypePtr& refTy = builder.refType(structTy->getTypeOfMember(id));
			if(call->getArgument(2)->getType() != refTy->getElementType() || call->getType() != refTy)
				res = builder.callExpr(refTy, fun, call->getArgument(0), call->getArgument(1),
					BASIC.getTypeLiteral(refTy->getElementType()));
		}

		if(BASIC.isSubscriptOperator(fun)) {
			const RefTypePtr& refTy = dynamic_pointer_cast<const RefType>(call->getArgument(0)->getType());
			const SingleElementTypePtr& seTy = static_pointer_cast<const SingleElementType>( refTy ? refTy->getElementType() : call->getArgument(0)->getType());
			const TypePtr& type = refTy ? builder.refType(seTy->getElementType()) : seTy->getElementType();

			if(call->getType() != type)
				res = builder.callExpr(type, fun, call->getArguments());
		}

		if(BASIC.isTupleRefElem(fun) || BASIC.isTupleMemberAccess(fun)) {
			ExpressionPtr arg = call->getArgument(2);
			int idx = -1;

			// search for the literal in the second argument
			auto lambdaVisitor = makeLambdaVisitor([&arg,this](const NodePtr& node) {
				// check for literal, assuming it will always be a valid integer
				if(const LiteralPtr& lit = dynamic_pointer_cast<const Literal>(node)) {
					std::cout << "TRY " << lit << std::endl;
					return atoi(lit->getValue().c_str());
				}
				return 0;
			});

			visitDepthFirst(call, lambdaVisitor);
		}
	}


	// check whether something has changed ...
	if (res == ptr) {
		// => nothing changed
		return ptr;
	}

	// preserve annotations
	utils::migrateAnnotations(ptr, res);

	// done
	return res;
}

}
}
}
}
