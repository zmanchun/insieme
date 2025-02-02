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
#ifndef __GUARD_ERROR_HANDLING_H
#define __GUARD_ERROR_HANDLING_H

#include "declarations.h"

#include <signal.h>
#include <stdio.h>

//#define IRT_VERBOSE 1

/* ------------------------------ data structures ----- */

typedef enum _irt_errcode {
#define IRT_ERROR(_err) _err,
#include "irt_errors.def"
} irt_errcode;

struct _irt_error {
	irt_errcode errcode;
	uint32 additional_bytes;
};

/* ------------------------------ operations ----- */

#if !defined(NDEBUG) || defined(IRT_VERBOSE) || defined(IRT_ENABLE_ASSERTS)
#ifdef _GEMS_SIM
/* lcc does not support macro called with 0 variadic arguments. The solution is to make the last named
 * argument part of the variadic arguments in a way to have always at least one variadic argument */
#define IRT_ASSERT(__condition, __errcode, /*__message,*/...)                                                                                                  \
	/* All following variadic macros used to have an explicit "message" parameter" which has been removed for this ^^ very reason */                           \
	if(!(__condition)) {                                                                                                                                       \
		fprintf(stderr, "IRT Assertion failure in %s#%d:\n", __FILE__, __LINE__);                                                                              \
		irt_throw_string_error(__errcode, __VA_ARGS__);                                                                                                        \
	}
#define IRT_WARN(/*__message,*/...)                                                                                                                            \
	{                                                                                                                                                          \
		fprintf(stderr, "IRT Warning in %s#%d:\n", __FILE__, __LINE__);                                                                                        \
		fprintf(stderr, __VA_ARGS__);                                                                                                                          \
		fprintf(stderr, "\n");                                                                                                                                 \
		fflush(stderr);                                                                                                                                        \
	}
#define IRT_INFO(...)                                                                                                                                          \
	{                                                                                                                                                          \
		printf(__VA_ARGS__);                                                                                                                                   \
		fflush(stdout);                                                                                                                                        \
	}
#ifdef IRT_VERBOSE
#define IRT_DEBUG_ONLY(__code__) __code__
#define IRT_DEBUG(...)                                                                                                                                         \
	{                                                                                                                                                          \
		printf("IRT Debug Info (%s#%d): ", __FILE__, __LINE__);                                                                                                \
		printf(__VA_ARGS__);                                                                                                                                   \
		printf("\n");                                                                                                                                          \
		fflush(stdout);                                                                                                                                        \
	}
#else
#define IRT_DEBUG_ONLY(__code__)
#define IRT_DEBUG(...)
#endif
#else
#define IRT_ASSERT(__condition, __errcode, ...)                                                                                                                \
	if(!(__condition)) {                                                                                                                                       \
		fprintf(stderr, "IRT Assertion failure in %s#%d:\n", __FILE__, __LINE__);                                                                              \
		irt_throw_string_error(__errcode, __VA_ARGS__);                                                                                                        \
	}
#define IRT_WARN(...)                                                                                                                                          \
	{                                                                                                                                                          \
		fprintf(stderr, "IRT Warning in %s#%d:\n", __FILE__, __LINE__);                                                                                        \
		fprintf(stderr, __VA_ARGS__);                                                                                                                          \
		fflush(stderr);                                                                                                                                        \
	}
#define IRT_INFO(...)                                                                                                                                          \
	{                                                                                                                                                          \
		printf(__VA_ARGS__);                                                                                                                                   \
		fflush(stdout);                                                                                                                                        \
	}
#ifdef IRT_VERBOSE
#define IRT_DEBUG_ONLY(__code__) __code__
#define IRT_DEBUG(...)                                                                                                                                         \
	{                                                                                                                                                          \
		printf("IRT Debug Info (%s#%d): ", __FILE__, __LINE__);                                                                                                \
		printf(__VA_ARGS__);                                                                                                                                   \
		fflush(stdout);                                                                                                                                        \
	}
#else
#define IRT_DEBUG_ONLY(__code__)
#define IRT_DEBUG(...)
#endif
#endif
#else
#define IRT_DEBUG_PRINTS_OFF
#define IRT_ASSERT(__condition, __errcode, ...)                                                                                                                \
	if(__condition)                                                                                                                                            \
		;
#define IRT_WARN(...)
#define IRT_INFO(...)
#define IRT_DEBUG(...)
#define IRT_DEBUG_ONLY(__code__)
#endif

#ifdef IRT_VERBOSE
#define IRT_VERBOSE_ONLY(__code) __code
#else
#define IRT_VERBOSE_ONLY(__code)
#endif

void irt_throw_string_error(irt_errcode code, const char* message, ...);

const char* irt_errcode_string(irt_errcode code);
void irt_print_error_info(FILE* target, irt_error* error);

#endif // ifndef __GUARD_ERROR_HANDLING_H
