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

#include "insieme/frontend/state/record_manager.h"

#include "insieme/frontend/utils/macros.h"

#include "insieme/utils/container_utils.h"

namespace insieme {
namespace frontend {
namespace state {

	core::GenericTypePtr RecordManager::lookup(const clang::RecordDecl* recordDecl) const {
		frontend_assert(::containsKey(records, recordDecl)) << "Trying to look up record not previously declared: " << dumpClang(recordDecl);
		return records.find(recordDecl)->second;
	}

	bool RecordManager::contains(const clang::RecordDecl* recordDecl) const {
		return ::containsKey(records, recordDecl);
	}

	void RecordManager::insert(const clang::RecordDecl* recordDecl, const core::GenericTypePtr& genType) {
		frontend_assert(!::containsKey(records, recordDecl)) << "Trying to insert previously declared record: " << dumpClang(recordDecl);
		records[recordDecl] = genType;
	}

} // end namespace state
} // end namespace frontend
} // end namespace insieme
