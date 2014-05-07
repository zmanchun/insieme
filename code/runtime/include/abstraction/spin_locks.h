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
#ifndef __GUARD_ABSTRACTION_SPIN_LOCKS_H
#define __GUARD_ABSTRACTION_SPIN_LOCKS_H

#include "irt_inttypes.h"

#if defined(_WIN32) && !defined(IRT_USE_PTHREADS)
	typedef long irt_spinlock;
#elif defined(_GEMS)
	typedef volatile int irt_spinlock;
#else
	#include <pthread.h>
	typedef pthread_spinlock_t irt_spinlock;
#endif

/** spin until lock is acquired */
inline void irt_spin_lock(irt_spinlock *lock);

/** release lock */
inline void irt_spin_unlock(irt_spinlock *lock);

/** initializing spin lock variable puts it in state unlocked. lock variable can not be shared by different processes */
inline int irt_spin_init(irt_spinlock *lock);

/**	destroy lock variable and free all used resources,
	will cause an error when attempting to destroy an object which is in any state other than unlocked */
inline void irt_spin_destroy(irt_spinlock *lock);

#endif // ifndef __GUARD_ABSTRACTION_SPIN_LOCKS_H
