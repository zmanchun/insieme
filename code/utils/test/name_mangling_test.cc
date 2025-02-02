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

#include "insieme/utils/name_mangling.h"

using namespace insieme::utils;

TEST(NameMangling, Basic) {
	EXPECT_EQ("IMP_bla", mangle("bla"));
	EXPECT_EQ("bla", demangle("bla"));
	EXPECT_EQ("bla", demangle("IMP_bla"));
	EXPECT_EQ("bla", demangle("IMP_bla_IMLOC_110_28"));
	EXPECT_EQ("IMP_kls_IMLOC__slash_bla_slash_xy_slash_z_dot_cpp_5_299", mangle("kls", "/bla/xy/z.cpp", 5, 299));
	EXPECT_EQ("kls", demangle("IMP_kls_IMLOC__slash_bla_slash_xy_slash_z_dot_cpp_5_299"));
}

TEST(NameMangling, Special) {
	EXPECT_EQ("IMP_bla_colon_klu_plus_r_wave__slash_", mangle("bla:klu+r~/"));
	EXPECT_EQ("bla:klu+r~/", demangle("IMP_bla_colon_klu_plus_r_wave__slash_"));
}

TEST(NameMangling, Empty) {
	EXPECT_EQ("IMP_EMPTY_IMLOC_foo_dot_cpp_42_7", mangle("", "foo.cpp", 42, 7));
	EXPECT_EQ("IMP_EMPTY_IMLOC_foo_dot_cpp_42_7", mangle("foo.cpp", 42, 7));
	EXPECT_EQ("", demangle("IMP_EMPTY_IMLOC_foo_dot_cpp_42_7"));
	EXPECT_EQ("IMP__not_really_mangle_empty__IMLOC_foo_dot_cpp_42_7", mangle("EMPTY", "foo.cpp", 42, 7));
	EXPECT_EQ("EMPTY", demangle("IMP__not_really_mangle_empty__IMLOC_foo_dot_cpp_42_7"));
}
