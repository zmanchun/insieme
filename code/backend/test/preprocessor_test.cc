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

#include <fstream>

#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/config.h"

#include "insieme/backend/preprocessor.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/checks/full_check.h"

#include "insieme/core/transform/node_replacer.h"

namespace insieme {
namespace backend {

	TEST(Preprocessor, GlobalElimination) {
		core::NodeManager manager;
		core::IRBuilder builder(manager);

		std::map<string, core::NodePtr> symbols;
		symbols["A"] = builder.structExpr(toVector<core::NamedValuePtr>(builder.namedValue("a", builder.getZero(builder.parseType("vector<int<4>,20>"))),
		                                                                builder.namedValue("f", builder.getZero(builder.parseType("real<8>")))));

		core::ProgramPtr program = builder.parseProgram(R"(
			alias gstruct = struct { a: vector<int<4>,20>; f : real<8>; };
			
			int<4> main() {
				var ref<gstruct> v1 = ref_new_init(A);
				v1.a;
				composite_member_access(*v1, lit("a"), type_lit(vector<int<4>,20>));
				(v2: ref<gstruct>) -> unit {
					v2.a;
					composite_member_access(*v2, lit("a"), type_lit(vector<int<4>,20>));
				} (v1);
				{
					v1.a = lit("X":vector<int<4>,20>);
				}
				return 0;
			}
            )",
		                                                symbols);

		EXPECT_TRUE(program);

		program = core::transform::fixTypesGen(manager, program, core::ExpressionMap(), false);

		//	std::cout << "Input: " << core::printer::PrettyPrinter(program) << "\n";

		// check for semantic errors
		auto errors = core::checks::check(program);
		EXPECT_EQ(core::checks::MessageList(), errors);
	}


} // namespace backend
} // namespace insieme
