/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include "insieme/frontend/utils/memalloc.h"

#include "insieme/core/analysis/ir_utils.h"

namespace insieme {
namespace frontend {
namespace utils {
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Special method which handle malloc and calloc which need to be treated in a special way in the IR.
	core::ExpressionPtr handleMemAlloc(const core::IRBuilder& builder, const core::TypePtr& type, const core::ExpressionPtr& subExpr) {
		core::CallExprPtr retVal;
		//if(core::CallExprPtr&& callExpr = core::dynamic_pointer_cast<const core::CallExpr>(subExpr)) {
		//	if(core::LiteralPtr&& lit = core::dynamic_pointer_cast<const core::Literal>(callExpr->getFunctionExpr())) {
		//		if(!(lit->getStringValue() == "malloc" || lit->getStringValue() == "calloc")) { return core::ExpressionPtr(); }

		//		assert(((lit->getStringValue() == "malloc" && callExpr->getArguments().size() == 1)
		//		        || (lit->getStringValue() == "calloc" && callExpr->getArguments().size() == 2))
		//		       && "malloc() and calloc() takes respectively 1 and 2 arguments");

		//		const core::lang::BasicGenerator& gen = builder.getLangBasic();
		//		// The type of the cast should be ref<array<'a>>, and the sizeof('a) need to be derived
		//		assert_eq(type->getNodeType(), core::NT_RefType);
		//		assert_eq(core::analysis::getReferencedType(type)->getNodeType(), core::NT_ArrayType);

		//		const core::RefTypePtr& refType = core::static_pointer_cast<const core::RefType>(type);
		//		const core::ArrayTypePtr& arrayType = refType->getElementType().as<core::ArrayTypePtr>();
		//		const core::TypePtr& elemType = arrayType->getElementType();

		//		/*
		//		 * The number of elements to be allocated of type 'targetType' is:
		//		 * 		-> 	expr / sizeof(targetType)
		//		 */
		//		core::CallExprPtr size;
		//		if(lit->getStringValue() == "malloc") {
		//			size = builder.callExpr(gen.getUInt8(), gen.getUnsignedIntDiv(), callExpr->getArgument(0), getSizeOfType(builder, elemType));
		//		} else {
		//			size = builder.callExpr(gen.getUInt8(), gen.getUnsignedIntDiv(), builder.mul(callExpr->getArgument(0), callExpr->getArgument(1)),
		//			                        getSizeOfType(builder, elemType));
		//		}

		//		auto memAlloc = builder.refNew(builder.callExpr(arrayType, gen.getArrayCreate1D(), builder.getTypeLiteral(elemType), size));

		//		if(lit->getStringValue() == "malloc") { return memAlloc; }
		//		// this is a calloc, then we have to do a memset to initialize the memory

		//		auto var = builder.variable(builder.refType(arrayType));
		//		auto declStmt = builder.declarationStmt(var, memAlloc);

		//		auto memSet =
		//		    builder.callExpr(builder.literal(builder.parseType("(ref<any>, int<4>, uint<8>) -> ref<any>"), "memset"), var, builder.intLit(0), size);

		//		return builder.createCallExprFromBody(builder.compoundStmt(declStmt, memSet, builder.returnStmt(var)), var->getType());
		//	}
		//}
		//return core::ExpressionPtr();
		assert_not_implemented() << "handleMemAlloc not implemented!";
		return retVal;
	}
}
}
}
