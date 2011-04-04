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

#pragma once

#include "globals.h"

#include <pthread.h>

#define IRT_DECLARE_ID_TYPE(__type) \
typedef struct _irt_##__type##_id irt_##__type##_id;

#define IRT_MAKE_ID_TYPE(__type) \
struct _irt_##__type; \
struct _irt_##__type##_id { \
	union { \
		uint64 full; \
		struct { \
			uint16 node; \
			uint16 thread; \
			uint32 index; \
		} components; \
	} value; \
	struct _irt_##__type* cached; \
}; \
static inline irt_##__type##_id irt_generate_##__type##_id() { \
	/* The hack below is necessary to maintain minimal dependencies. \
	This way, id_generation.h does not need to know about workers */ \
	uint64* worker_ptr = (uint64*)pthread_getspecific(irt_g_worker_key); \
	irt_##__type##_id id; \
	id.value.full = *worker_ptr; \
	/* access the generator index which is 128 bits after the start of the worker struct */ \
	id.value.components.index = (*(uint32*)(worker_ptr+2))++; \
	id.cached = NULL; \
	return id; \
}


