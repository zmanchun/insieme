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

#include <gtest/gtest.h>

#include "insieme/analysis/features/code_features.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/parser/ir_parse.h"

namespace insieme {
namespace analysis {
namespace features {

	using namespace core;

	TEST(NumStatements, Basic) {
		NodeManager mgr;
		parse::IRParser parser(mgr);
		auto& basic = mgr.getLangBasic();

		auto forStmt = static_pointer_cast<const ForStmt>( parser.parseStatement("\
			for(decl uint<4>:i = 10 .. 50 : 1) { \
				(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, i)); \
				for(decl uint<4>:j = 5 .. 25 : 1) { \
					(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (i+j))); \
				    (op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (i+j))); \
				}; \
			}") );


		EXPECT_TRUE(forStmt);

		// run dummy test ...
		EXPECT_EQ(100300, countOps(forStmt));

		// check number of adds
		EXPECT_EQ(0, countOps(forStmt, basic.getSignedIntAdd()));
		EXPECT_EQ(20100, countOps(forStmt, basic.getArrayRefElem1D()));
		EXPECT_EQ(20000, countOps(forStmt, basic.getUnsignedIntAdd()));
	}


} // end namespace features
} // end namespace analysis
} // end namespace insieme
