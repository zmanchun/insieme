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

#include "independent_test_utils.h"

namespace insieme {
namespace frontend {

	TEST(IndependentTest, Literals) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_literals.c");
	}
	
	TEST(IndependentTest, BasicTypes) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_basic_types.c");
	}

	TEST(IndependentTest, Globals) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_globals.c");
	}

	TEST(IndependentTest, GlobalsInit) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_globals_init.c");
	}

	TEST(IndependentTest, Statements) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_statements.c");
	}

	TEST(IndependentTest, VariableScopes) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_variable_scopes.c");
	}
	
	TEST(IndependentTest, FunCalls) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_fun_calls.c");
	}

	TEST(IndependentTest, Expressions) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_expressions.c");
	}

	TEST(IndependentTest, Casts) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_casts.c");
	}

	TEST(IndependentTest, DeclInitExpressions) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_decl_init_expressions.c");
	}

	TEST(IndependentTest, Prototypes) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_prototypes.c");
	}

	TEST(IndependentTest, FunctionPointers) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_function_pointers.c");
	}
	
	TEST(IndependentTest, Static) {
		runIndependentTestOn(FRONTEND_TEST_DIR "/inputs/conversion/c_static.c");
	}

} // fe namespace
} // insieme namespace
