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

#include "insieme/core/transform/sequentialize.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_address.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/analysis/normalize.h"

#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/test/test_utils.h"

#include "insieme/core/lang/parallel.h"

namespace insieme {
namespace core {
namespace transform {

	TEST(Manipulation, SequentializeBug) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto op = mgr.getLangExtension<lang::ParallelExtension>().getAtomicFetchAndAdd();
		EXPECT_TRUE(lang::isBuiltIn(op));

		auto seq = transform::sequentialize(mgr, op);

		EXPECT_NE(op, seq);
		EXPECT_FALSE(lang::isBuiltIn(seq));

	}

	TEST(Manipulation, SequentializeAtomic) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		StatementPtr code = analysis::normalize(builder.parseStmt("alias int = int<4>;"
		                                                          "{"
		                                                          "	var ref<int> a = ref_var_init(2);"
		                                                          "	atomic_fetch_and_add(a, 10);"
		                                                          "}")
		                                            .as<StatementPtr>());

		ASSERT_TRUE(code);

		EXPECT_EQ("{decl ref<int<4>,f,f,plain> v0 =  ref_var_init(2);atomic_fetch_and_add(v0, 10);}",
		          toString(printer::PrettyPrinter(code, printer::PrettyPrinter::PRINT_SINGLE_LINE)));
		EXPECT_TRUE(check(code, checks::getFullCheck()).empty()) << check(code, checks::getFullCheck());


		auto res = analysis::normalize(transform::trySequentialize(mgr, code));
		//		std::cout << core::printer::PrettyPrinter(res) << "\n";
		//SHOULD be somehow like: EXPECT_EQ("{var ref<int<4>,f,f,plain> v0 =  ref_var(2);function(v1 : ref<ref<'a,f,'v,plain>,f,f,plain>, v2 : ref<'a,f,f,plain>) -> 'a {return (v1 : ref<'a,f,'v,plain>,f,f,plain, v2 : 'a) -> 'a {var 'a v3 = v1;v1 = gen_add(v1, v2);return v3;}(v1, v2);}(v0, 10);}",
		EXPECT_EQ("{decl ref<int<4>,f,f,plain> v0 =  ref_var_init(2);function(ref<ref<'a,f,'v,plain>,f,f,plain> v0, ref<'a,f,f,plain> v1) -> 'a {function('a v2)=> id(true);function('a v3)=> gen_add(v3, v1);return function(ref<ref<'a,f,'v,plain>,f,f,plain> v0, ref<'a,f,f,plain> v1) -> 'a {decl 'a v2 = v0;v0 = gen_add(v0, v1);return v2;}(v0, v1);}(v0, 10);}",
		          toString(printer::PrettyPrinter(res, printer::PrettyPrinter::PRINT_SINGLE_LINE)));
		EXPECT_TRUE(check(res, checks::getFullCheck()).empty()) << check(res, checks::getFullCheck());
	}

} // end namespace transform
} // end namespace core
} // end namespace insieme
