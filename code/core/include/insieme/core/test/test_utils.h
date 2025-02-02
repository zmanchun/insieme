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

#pragma once

#include <string>
#include <gtest/gtest.h>

#include "insieme/utils/string_utils.h"
#include "insieme/core/ir_node.h"
#include "insieme/core/checks/full_check.h"

#include "insieme/core/lang/extension.h"
#include "insieme/core/printer/error_printer.h"

using std::string;

namespace insieme {
namespace core {

	// -- A set of useful functions when implementing test cases which use core classes -------------

	/**
	 * Performs semantic checks for all second elements in a map. Uses gtest EXPECT to verify that no errors occur.
	 *
	 * @param map The map which's second element should be checked
	 */
	template <typename T>
	void semanticCheckSecond(const std::map<T, lang::lazy_factory>& map) {
		for(auto cur : map) {
			// create node
			auto node = cur.second();

			// check node
			auto errors = checks::check(node);

			// just check whether the code is not exhibiting errors
			EXPECT_TRUE(errors.empty()) <<
					"Key:    " << cur.first << "\n"
					"Code:   " << *node  << "\n" <<
					"Errors: " << errors;
		}
	}
}
}
