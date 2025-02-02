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

#include <gtest/gtest.h>

#include "insieme/core/lang/pointer.h"
#include "insieme/core/test/test_utils.h"

#include "insieme/core/ir_builder.h"

namespace insieme {
namespace core {
namespace lang {

	TEST(Pointer, SemanticChecks) {
		NodeManager nm;
		auto& ext = nm.getLangExtension<PointerExtension>();
		semanticCheckSecond(ext.getSymbols());
	}

	TEST(Pointer, StructSubstitute) {
		NodeManager nm;
		const PointerExtension& ext = nm.getLangExtension<PointerExtension>();

		// the generic pointer template should be a struct
		EXPECT_EQ(NT_TupleType, ext.getGenPtr()->getNodeType());

		// the arguments in the functions accepting a pointer should be expanded to a struct
		EXPECT_EQ(NT_TupleType, ext.getPtrCast()->getType().as<FunctionTypePtr>()->getParameterTypes()[0]->getNodeType());
	}

	TEST(Pointer, Alias) {

		// test whether the pointer alias is working

		NodeManager nm;
		IRBuilder builder(nm);

		auto t1 = builder.parseType("ptr<int<4>>");
		auto t2 = builder.parseType("ptr<int<4>,f,f>");
		auto t3 = builder.parseType("ptr<int<4>,f,t>");

		EXPECT_EQ(t1->getNodeType(), NT_TupleType);
		EXPECT_EQ(t2->getNodeType(), NT_TupleType);
		EXPECT_EQ(t3->getNodeType(), NT_TupleType);

		EXPECT_EQ(t1, t2);
		EXPECT_NE(t1, t3);
	}

	TEST(Pointer, IsPointer) {
		NodeManager nm;
		IRBuilder builder(nm);

		auto A = builder.parseType("A");
		EXPECT_TRUE(isPointer(PointerType::create(A)));
		EXPECT_TRUE(isPointer(PointerType::create(A, false, true)));

		EXPECT_FALSE(isPointer(builder.parseType("A")));
		EXPECT_TRUE(isPointer(builder.parseType("ptr<A>")));
		EXPECT_TRUE(isPointer(builder.parseType("ptr<A,f,t>")));

		EXPECT_TRUE(isPointer(builder.parseType("ptr<A,f,t>")));

		EXPECT_FALSE(isPointer(builder.parseType("ptr<A,c,t>")));
		EXPECT_FALSE(isPointer(builder.parseType("ptr<A,f,t,c>")));
	}

} // end namespace lang
} // end namespace core
} // end namespace insieme
