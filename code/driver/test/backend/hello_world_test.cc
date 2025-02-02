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

#include "insieme/driver/integration/tests.h"

#include "insieme/utils/compiler/compiler.h"

#include "insieme/frontend/frontend.h"
#include "insieme/core/ir_node.h"
#include "insieme/core/ir_program.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/backend/sequential/sequential_backend.h"

namespace insieme {
namespace backend {


	TEST(FullBackend, HelloWorld) {
		core::NodeManager manager;

		// load hello world test case
		auto testCase = driver::integration::getCase("hello_world");
		ASSERT_TRUE(testCase) << "Could not load hello world test case!";

		// convert test case into IR using the frontend
		auto job = frontend::ConversionJob(testCase->getFiles(), testCase->getIncludeDirs());
		job.registerDefaultExtensions();
		auto code = job.execute(manager);
		ASSERT_TRUE(code) << "Unable to load input code!";

		// create target code using real backend
		auto target = backend::sequential::SequentialBackend::getDefault()->convert(code);

		// check target code
		//	EXPECT_EQ("", toString(core::printer::PrettyPrinter(code, core::printer::PrettyPrinter::OPTIONS_DETAIL)));
		//	EXPECT_EQ("", toString(*target));

		// see whether target code can be compiled
		// TODO: compile target code => test result
		//	EXPECT_TRUE(utils::compiler::compile(*target));
	}

} // end namespace backend
} // end namespace insieme
