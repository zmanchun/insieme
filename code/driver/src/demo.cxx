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

/**
 * Within this file a small, simple example of a compiler driver utilizing
 * the insieme compiler infrastructure is presented.
 *
 * This file is intended to provides a template for implementing new compiler
 * applications utilizing the Insieme compiler and runtime infrastructure.
 */

#include <string>

#include "insieme/utils/logging.h"
#include "insieme/utils/compiler/compiler.h"
#include "insieme/frontend/frontend.h"
#include "insieme/backend/runtime/runtime_backend.h"
#include "insieme/driver/cmd/insiemecc_options.h"

#include "insieme/transform/connectors.h"
#include "insieme/transform/filter/standard_filter.h"
#include "insieme/transform/rulebased/transformations.h"

using namespace std;

namespace fe = insieme::frontend;
namespace co = insieme::core;
namespace be = insieme::backend;
namespace ut = insieme::utils;
namespace cp = insieme::utils::compiler;
namespace tr = insieme::transform;
namespace cmd = insieme::driver::cmd;


int main(int argc, char** argv) {
	// Step 1: parse input parameters
	//		This part is application specific and need to be customized. Within this
	//		example a few standard options are considered.
	unsigned unrollingFactor = 1;
	std::vector<std::string> arguments(argv, argv + argc);
	cmd::Options options = cmd::Options::parse(arguments)
	    // one extra parameter - unrolling factor, default should be 5
	    ("unrolling,u", unrollingFactor, 5u, "The factor by which the innermost loops should be unrolled.");
	if(!options.valid) { return (options.settings.help) ? 0 : 1; }


	// Step 2: load input code
	//		The frontend is converting input code into the internal representation (IR).
	//		The memory management of IR nodes is realized using node manager instances.
	//		The life cycle of IR nodes is bound to the manager the have been created by.
	co::NodeManager manager;
	auto program = options.job.execute(manager);


	// Step 3: process code
	//		This is the part where the actual operations on the processed input code
	//		are conducted. You may utilize whatever functionality provided by the
	//		compiler framework to analyze and manipulate the processed application.
	//		In this example we are simply unrolling all innermost loops by a factor
	//		of 5 which is always a safe transformation.

	cout << "Before Transformation:\n";
	cout << dumpPretty(program);

	// for all nodes x | if x is "innermostLoop" => unroll(x)
	auto transform = tr::makeForAll(tr::filter::innermostLoops(), tr::rulebased::makeLoopUnrolling(unrollingFactor));
	program = transform->apply(program);

	cout << "After Transformation:\n";
	cout << dumpPretty(program);


	// Step 4: produce output code
	//		This part converts the processed code into C-99 target code using the
	//		backend producing parallel code to be executed using the Insieme runtime
	//		system. Backends targeting alternative platforms may be present in the
	//		backend modul as well.
	cout << "Creating target code ...\n";
	auto targetCode = be::runtime::RuntimeBackend::getDefault()->convert(program);


	// Step 5: build output code
	//		A final, optional step is using a third-party C compiler to build an actual
	//		executable.
	cout << "Building binaries ...\n";
	cp::Compiler compiler = cp::Compiler::getDefaultC99Compiler();
	compiler = cp::Compiler::getOptimizedCompiler(compiler);
	compiler = cp::Compiler::getRuntimeCompiler(compiler);
	bool success = cp::compileToBinary(*targetCode, options.settings.outFile.string(), compiler);

	// done
	return (success) ? 0 : 1;
}
