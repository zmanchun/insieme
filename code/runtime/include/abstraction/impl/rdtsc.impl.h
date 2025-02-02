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

// platform dependent implmentations of functions using rdtsc

#pragma once
#ifndef __GUARD_ABSTRACTION_IMPL_RDTSC_IMPL_H
#define __GUARD_ABSTRACTION_IMPL_RDTSC_IMPL_H

#include "abstraction/rdtsc.h"

// no general variant by design to raise compiler errors in case of a new architecture

#if defined(__x86_64__) || defined(__i386__) /*GNU C x86*/ || defined(_M_IX86) /*VS x86*/ || defined(_GEMS_SIM) || defined(__arm__)

// ====== all x86 based platforms, 32 and 64bit =====

#if defined(_MSC_VER)
#include "abstraction/impl/rdtsc.win.impl.h"
#elif defined(__MINGW32__)
#include "abstraction/impl/rdtsc.mingw.impl.h"
#elif defined(_GEMS_SIM)
#include "abstraction/impl/rdtsc.gems.impl.h"
#elif defined(__arm__)
#include "abstraction/impl/rdtsc.arm.impl.h"
#else
#include "abstraction/impl/rdtsc.unix.impl.h"
#endif

#elif defined(__powerpc__)

#warning "Incomplete implementation of rdtsc functionality for power pc!"

// ====== PowerPC machines ==========================
// deliberately fails for new architectures

uint64 irt_time_ticks(void) {
	int64 upper0, upper1, lower;

	__asm__ volatile("\
		mfspr %[upper0], 269 \n\
		mfspr %[lower] , 268 \n\
		mfspr %[upper1], 269 "
	                 : [lower] "=r"(lower), [upper0] "=r"(upper0), [upper1] "=r"(upper1));

	return (uint64)(((upper1 ^ ((upper0 ^ upper1) & (lower >> 31))) << 32) | lower);
}

bool irt_time_ticks_constant() {
	return true;
}

#endif


#endif // ifndef __GUARD_ABSTRACTION_IMPL_RDTSC_IMPL_H
