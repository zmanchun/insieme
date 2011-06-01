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

#include "work_item.h"

#include <stdlib.h>
#include <alloca.h>

#include "impl/worker.impl.h"
#include "utils/impl/minlwt.impl.h"
#include "irt_atomic.h"
#include "work_group.h"
#include "impl/error_handling.impl.h"
#include "impl/irt_scheduling.impl.h"


static inline irt_work_item* irt_wi_get_current() {
	return irt_worker_get_current()->cur_wi;
}

static inline irt_work_item* _irt_wi_new() {
	return (irt_work_item*)malloc(sizeof(irt_work_item));
}
static inline void _irt_wi_recycle(irt_work_item* wi) {
	free(wi);
}

static inline void _irt_wi_allocate_wgs(irt_work_item* wi) {
	wi->work_groups = (irt_work_group_id*)malloc(sizeof(irt_work_group_id)*IRT_MAX_WORK_GROUPS);
}

static inline void _irt_print_work_item_range(const irt_work_item_range* r) { 
	IRT_INFO("%ld..%ld : %ld", r->begin, r->end, r->step);
}

irt_work_item* irt_wi_create(irt_work_item_range range, irt_wi_implementation_id impl_id, irt_lw_data_item* params) {
	irt_work_item* retval = _irt_wi_new();
	retval->id = irt_generate_work_item_id(IRT_LOOKUP_GENERATOR_ID_PTR);
	retval->id.cached = retval;
	retval->impl_id = impl_id;
	retval->context_id = irt_worker_get_current()->cur_context;
	retval->num_groups = 0;
	retval->work_groups = NULL;
	retval->parameters = params;
	retval->range = range;
	retval->state = IRT_WI_STATE_NEW;
	retval->stack_start = 0;
	//retval->stack_ptr = 0;
	retval->source_id = irt_work_item_null_id();
	retval->num_fragments = 0;
	return retval;
}
irt_work_item* _irt_wi_create_fragment(irt_work_item* source, irt_work_item_range range) {
	irt_work_item* retval = _irt_wi_new();
	memcpy(retval, source, sizeof(irt_work_item));
	retval->id = irt_generate_work_item_id(IRT_LOOKUP_GENERATOR_ID_PTR);
	retval->id.cached = retval;
	retval->num_fragments = 0;
	retval->range = range;
	if(irt_wi_is_fragment(source)) {
		// splitting fragment wi
		irt_work_item *base_source = source->source_id.cached; // TODO
		retval->source_id = base_source->id;
	} else {
		// splitting non-fragment wi
		retval->source_id = source->id;
	}
	return retval;
}
void irt_wi_destroy(irt_work_item* wi) {
	_irt_wi_recycle(wi);
}

bool _irt_wi_done_check(irt_work_item* wi) {
	return ((irt_work_item*)(wi->ready_check.data))->state == IRT_WI_STATE_DONE;
}
typedef struct __irt_wi_multi_check_closure {
	uint32 cur_wi;
	uint32 num_wis;
	irt_work_item** wis;
} _irt_wi_multi_check_closure;
bool _irt_wi_multi_done_check(irt_work_item* wi) {
	_irt_wi_multi_check_closure* closure = (_irt_wi_multi_check_closure*)wi->ready_check.data;
	for(int i = closure->cur_wi; i < closure->num_wis; ++i) {
		if(closure->wis[i]->state != IRT_WI_STATE_DONE) {
			closure->cur_wi = i;
			return false;
		}
	}
	return true;
}

void irt_wi_join(irt_work_item* wi) {
	irt_worker* self = irt_worker_get_current();
	irt_work_item* swi = self->cur_wi;
	swi->ready_check.fun = &_irt_wi_done_check;
	swi->ready_check.data = wi;
	irt_scheduling_yield(self, swi);
}
void irt_wi_multi_join(uint32 num_wis, irt_work_item** wis) {
	irt_worker* self = irt_worker_get_current();
	irt_work_item* swi = self->cur_wi;
	swi->ready_check.fun = &_irt_wi_multi_done_check;
	_irt_wi_multi_check_closure closure = { 0, num_wis, wis };
	swi->ready_check.data = &closure;
	irt_scheduling_yield(self, swi);
}

void irt_wi_end(irt_work_item* wi) {
	wi->state = IRT_WI_STATE_DONE;
	IRT_DEBUG("Wi %p / Worker %p irt_wi_end.", wi, irt_worker_get_current());
	irt_worker *worker = irt_worker_get_current();
	worker->cur_wi = NULL;
	if(irt_wi_is_fragment(wi)) {
		// ended wi was a fragment
		irt_work_item *source = wi->source_id.cached; // TODO
		IRT_DEBUG("Fragment end, remaining %d", source->num_fragments);
		irt_atomic_fetch_and_sub(&source->num_fragments, 1);
		if(source->num_fragments == 0) irt_wi_end(source);
	}
	lwt_end(&worker->basestack);
	IRT_ASSERT(false, IRT_ERR_INTERNAL, "NEVERMORE");
}

void irt_wi_split_uniform(irt_work_item* wi, uint32 elements, irt_work_item** out_wis) {
	// TODO implement custom (faster)
	irt_work_item_range *r = &wi->range;
	uint64 *offsets = (uint64*)alloca(sizeof(uint64)*elements);
	uint64 step = (r->end - r->begin) / elements, cur = r->begin;
	for(int i=0; i<elements; ++i, cur+=step) offsets[i] = cur;
	irt_wi_split(wi, elements, offsets, out_wis);
}
void irt_wi_split_binary(irt_work_item* wi, irt_work_item* out_wis[2]) {
	// TODO implement custom (faster)
	irt_work_item_range *r = &wi->range;
	uint64 offsets[] = {r->begin, r->begin + ( (r->end - r->begin) / 2)};
	irt_wi_split(wi, 2, offsets, out_wis);
}
void irt_wi_split(irt_work_item* wi, uint32 elements, uint64* offsets, irt_work_item** out_wis) {
	irt_work_item_range range = wi->range;
	for(int i=0; i<elements; ++i) {
		range.begin = offsets[i];
		range.end = i+1 < elements ? offsets[i+1] : wi->range.end;
		out_wis[i] = _irt_wi_create_fragment(wi, range);
	}
	
	if(irt_wi_is_fragment(wi)) {
		irt_work_item* source = wi->source_id.cached; // TODO
		irt_atomic_fetch_and_add(&source->num_fragments, elements - 1); // This needs to be atomic even if it may not look like it
		for(uint32 i=0; i<source->num_groups; ++i) {
			irt_atomic_fetch_and_add(&(source->work_groups[i].cached->local_member_count), elements - 1); // TODO
		}
		// splitting fragment wi, can safely delete
		_irt_wi_recycle(wi);
	} else {
		irt_atomic_fetch_and_add(&wi->num_fragments, elements); // This needs to be atomic even if it may not look like it		
		for(uint32 i=0; i<wi->num_groups; ++i) {
			irt_atomic_fetch_and_add(&(wi->work_groups[i].cached->local_member_count), elements - 1); // TODO
		}
	}
}
