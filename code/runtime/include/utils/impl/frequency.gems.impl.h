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
#ifndef __GUARD_UTILS_IMPL_FREQUENCY_GEMS_H
#define __GUARD_UTILS_IMPL_FREQUENCY_GEMS_H


/*
 * These functions provide an interface to get and set CPU frequency settings.
 */

#define IRT_GEM_MIN_FREQ    (1 * 1000)      // 1 MHz
#define IRT_GEM_MAX_FREQ    (101 * 1000)    // 101 MHz
#define IRT_GEM_STEP_FREQ   (5 * 1000)      // 5 MHz


int32 irt_cpu_freq_get_available_frequencies(uint32* frequencies, uint32* length){
#ifdef _GEM_SIM
    uint32 freq = IRT_GEM_MIN_FREQ;
    uint32 j = 0;

    while(freq <= IRT_GEM_MAX_FREQ) {
        frequencies[j++] = freq;
        freq += IRT_GEM_STEP_FREQ;
    }

    *length = j;
#else
    for(*length=0; *length<9; (*length)++)
        frequencies[*length] = freq_table_a15[*length];
#endif

    return 0;
} 

/*
 * reads all available frequencies for a specific core as a list into the provided pointer
 */

int32 irt_cpu_freq_get_available_frequencies_core(const uint32 coreid, uint32* frequencies, uint32* length) {
    IRT_ASSERT(irt_affinity_mask_get_first_cpu(irt_worker_get_current()->affinity) == coreid,
        IRT_ERR_INVALIDARGUMENT, "DVFS of non-current core is unsupported");
    return irt_cpu_freq_get_available_frequencies(frequencies, length);
}

/*
 * reads all available frequencies for a worker running on a specific core as a list into the provided pointer
 */

int32 irt_cpu_freq_get_available_frequencies_worker(const irt_worker* worker, uint32* frequencies, uint32* length) {
    IRT_ASSERT(irt_worker_get_current() == worker,
        IRT_ERR_INVALIDARGUMENT, "DVFS of non-current worker is unsupported");
    return irt_cpu_freq_get_available_frequencies(frequencies, length);
}

/*
 * gets the current frequency a core of a worker is running at
 */

int32 irt_cpu_freq_get_cur_frequency_worker(const irt_worker* worker){
    IRT_ASSERT(irt_worker_get_current() == worker,
        IRT_ERR_INVALIDARGUMENT, "DVFS of non-current worker is unsupported");

    return rapmi_get_freq();
}  

/*
 * sets the frequency of a core of a worker to a specific value by setting both the min and max to this value
 */

int32 irt_cpu_freq_set_frequency_worker(const irt_worker* worker, const uint32 frequency){
    IRT_ASSERT(irt_worker_get_current() == worker,
        IRT_ERR_INVALIDARGUMENT, "DVFS of non-current worker is unsupported");

    return rapmi_set_freq(frequency);
}

/*
 * sets the maximum frequency a core of a worker is allowed to run at
 */

bool irt_cpu_freq_set_max_frequency_worker(const irt_worker* worker, const uint32 frequency){
    /* No limits on the gemsclaim simulator */
    return (IRT_GEM_MAX_FREQ >= frequency);
} 

/*
 * gets the maximum frequency a core of a worker is allowed to run at
 */

int32 irt_cpu_freq_get_max_frequency_worker(const irt_worker* worker){ 
    return IRT_GEM_MAX_FREQ;
} 

/*
 * gets the maximum frequency a core is allowed to run at
 */

int32 irt_cpu_freq_get_max_frequency_core(const uint32 coreid){ 
    return IRT_GEM_MAX_FREQ;
} 

/*
 * sets the minimum frequency a core of a worker is allowed to run at
 */

bool irt_cpu_freq_set_min_frequency_worker(const irt_worker* worker, const uint32 frequency){
    /* No limits on the gemsclaim simulator */
    return (IRT_GEM_MIN_FREQ <= frequency);
}

/*
 * gets the minimum frequency a core of a worker is allowed to run at
 */

int32 irt_cpu_freq_get_min_frequency_worker(const irt_worker* worker){
    return IRT_GEM_MIN_FREQ;
}

/*
 * gets the minimum frequency a core is allowed to run at
 */

int32 irt_cpu_freq_get_min_frequency_core(const uint32 coreid){ 
    return IRT_GEM_MIN_FREQ;
}

/*
 * resets all the min and max frequencies of all cores of all workers to the available min and max reported by the hardware
 */

bool irt_cpu_freq_reset_frequencies(){
    /* No limits on the gemsclaim simulator */
    return true;
} 

/*
 * resets the min and max frequencies of a core of a worker to the available min and max
 */

int32 irt_cpu_freq_reset_frequency_worker(const irt_worker* worker){
    /* No limits on the gemsclaim simulator */

    return (IRT_GEM_MAX_FREQ - IRT_GEM_MIN_FREQ) / IRT_GEM_STEP_FREQ + 1;
}

#endif // ifndef __GUARD_UTILS_IMPL_FREQUENCY_GEMS_H
