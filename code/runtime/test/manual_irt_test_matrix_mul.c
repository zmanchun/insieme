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

#include "impl/client_app.impl.h"
#include "impl/irt_context.impl.h"
#include "impl/error_handling.impl.h"
#include "impl/worker.impl.h"
#include "impl/irt_mqueue.impl.h"
#include "impl/data_item.impl.h"
#include "irt_types.h"
#include "wi_implementation.h"
#include "utils/timing.h"

#define N 1000

#define INSIEME_BOOL_T_INDEX 0
#define INSIEME_DOUBLE_T_INDEX 1
#define INSIEME_DATA_ITEM_ID_T_INDEX 2
#define INSIEME_TYPE_ID_T_INDEX 3
#define INSIEME_WI_INIT_PARAM_T_INDEX 4
#define INSIEME_WI_MUL_PARAM_T_INDEX 5

typedef struct _insieme_wi_init_params {
	irt_type_id type;
	irt_data_item_id A;
	irt_data_item_id B;
} insieme_wi_init_params;

typedef struct _insieme_wi_mul_params {
	irt_type_id type;
	irt_data_item_id A;
	irt_data_item_id B;
	irt_data_item_id C;
} insieme_wi_mul_params;

// type table

irt_type_id g_insieme_init_params_subtypes[] = {
	INSIEME_TYPE_ID_T_INDEX, INSIEME_DATA_ITEM_ID_T_INDEX, INSIEME_DATA_ITEM_ID_T_INDEX // struct including 2 data item ids
};

irt_type_id g_insieme_mul_params_subtypes[] = {
	INSIEME_TYPE_ID_T_INDEX, INSIEME_DATA_ITEM_ID_T_INDEX, INSIEME_DATA_ITEM_ID_T_INDEX, INSIEME_DATA_ITEM_ID_T_INDEX // struct including 3 data item ids
};

// type table:
// # kind, size, number of sub-elements, array of sub-elements
irt_type g_insieme_type_table[] = {
	{ IRT_T_BOOL, sizeof(int), 0, 0 },
	{ IRT_T_REAL64, sizeof(double), 0, 0 },
	{ IRT_T_BASIC,  sizeof(irt_data_item_id), 0, 0 },
	{ IRT_T_BASIC,  sizeof(irt_type_id), 0, 0 },
	{ IRT_T_STRUCT, sizeof(insieme_wi_init_params), 2, g_insieme_init_params_subtypes },
	{ IRT_T_STRUCT, sizeof(insieme_wi_mul_params), 3, g_insieme_mul_params_subtypes },
};

// work item table

void insieme_wi_startup_implementation(irt_work_item* wi);

void insieme_wi_init_implementation(irt_work_item* wi);
void insieme_wi_init_datareq(irt_work_item* wi, irt_wi_di_requirement* requirements);

void insieme_wi_mul_implementation1(irt_work_item* wi);
void insieme_wi_mul_implementation2(irt_work_item* wi);
void insieme_wi_mul_datareq(irt_work_item* wi, irt_wi_di_requirement* requirements);

irt_wi_implementation_variant g_insieme_wi_startup_variants[] = {
	{ &insieme_wi_startup_implementation, 0, NULL, 0, NULL }
};

irt_wi_implementation_variant g_insieme_wi_init_variants[] = {
	{ &insieme_wi_init_implementation, 4, &insieme_wi_init_datareq, 0, NULL }
};

irt_wi_implementation_variant g_insieme_wi_mul_variants[] = {
	{ &insieme_wi_mul_implementation1, 6, &insieme_wi_mul_datareq, 0, NULL },
	{ &insieme_wi_mul_implementation2, 6, &insieme_wi_mul_datareq, 0, NULL }
};

#define INSIEME_WI_INIT_INDEX 1
#define INSIEME_WI_MUL_INDEX 2

// The implementation table:
// # of variants, array of variants
irt_wi_implementation g_insieme_impl_table[] = {
	{ 1, g_insieme_wi_startup_variants },
	{ 1, g_insieme_wi_init_variants },
	{ 2, g_insieme_wi_mul_variants }
};

// initialization
void insieme_init_context(irt_context* context) {
	context->type_table = g_insieme_type_table;
	context->impl_table = g_insieme_impl_table;
}

void insieme_cleanup_context(irt_context* context) {
	// nothing
	printf("Cleaning up manual IRT test matrix mul\n");
}

// work item function definitions

void insieme_wi_startup_implementation(irt_work_item* wi) {

	// create data arrays
	irt_data_range range[] = {{0,N,1},{0,N,1}};
	irt_data_item* A = irt_di_create(INSIEME_DOUBLE_T_INDEX, 2, range);
	irt_data_item* B = irt_di_create(INSIEME_DOUBLE_T_INDEX, 2, range);
	irt_data_item* C = irt_di_create(INSIEME_DOUBLE_T_INDEX, 2, range);

	// measure the time
	uint64 start_time = irt_time_ms();

	// create and run initialization job
	insieme_wi_init_params init_params = {INSIEME_WI_INIT_PARAM_T_INDEX, A->id, B->id};
	irt_work_item* init_wi = irt_wi_create((irt_work_item_range){0,N,1}, INSIEME_WI_INIT_INDEX, (irt_lw_data_item*)&init_params);
	irt_worker_enqueue(irt_worker_get_current(), init_wi);

	// wait until finished
	irt_wi_join(init_wi);


	// conduct the multiplication
	insieme_wi_mul_params mul_params = {INSIEME_WI_MUL_PARAM_T_INDEX, A->id, B->id, C->id};
	irt_work_item* mul_wi = irt_wi_create((irt_work_item_range){0,N,1}, INSIEME_WI_MUL_INDEX, (irt_lw_data_item*)&mul_params);
	irt_worker_enqueue(irt_worker_get_current(), mul_wi);

	// wait until finished
	irt_wi_join(mul_wi);

	// stop the time
	uint64 end_time = irt_time_ms();


	// check correctness

	irt_data_range subrange[] = {{0,N,1},{0,N,1}};
	irt_data_item* itemR = irt_di_create_sub(irt_data_item_table_lookup(C->id), subrange);
	irt_data_block* blockR = irt_di_aquire(itemR, IRT_DMODE_READ_ONLY);
	double** R = (double**)blockR->data;

	printf("======================\n= manual irt test matrix multiplication\n");
	printf("= time taken: %lu\n", end_time - start_time);
	bool check = true;
	for (int i=0; i<N; i++) {
		for (int j=0; j<N; j++) {
			if (R[i][j] != i*j) {
				check = false;
				// printf("= fail at (%d,%d) - expected %d / actual %f\n", i, j, i*j, R[i][j]);
			}
		}
	}
	printf("= result check: %s\n======================\n", check ? "OK" : "FAIL");

	irt_di_free(blockR);
	irt_di_destroy(itemR);

	// cleanup
	irt_di_destroy(A);
	irt_di_destroy(B);
	irt_di_destroy(C);

	// terminate this work item
	irt_wi_end(wi);
}

void insieme_wi_mul_implementation1(irt_work_item* wi) {

	// get parameters
	insieme_wi_mul_params *params = (insieme_wi_mul_params*)wi->parameters;

	irt_work_item_range range = wi->range;
	irt_data_range subrange[] = {{range.begin, range.end, range.step}, {0,N,1}};
	irt_data_range fullrange[] = {{0,N,1}, {0,N,1}};

	irt_data_item* itemA = irt_di_create_sub(irt_data_item_table_lookup(params->A), subrange);
	irt_data_item* itemB = irt_di_create_sub(irt_data_item_table_lookup(params->B), fullrange);
	irt_data_item* itemC = irt_di_create_sub(irt_data_item_table_lookup(params->C), subrange);

	irt_data_block* blockA = irt_di_aquire(itemA, IRT_DMODE_READ_ONLY);
	irt_data_block* blockB = irt_di_aquire(itemB, IRT_DMODE_READ_ONLY);
	irt_data_block* blockC = irt_di_aquire(itemC, IRT_DMODE_WRITE_FIRST);

	double** A = (double**)blockA->data;
	double** B = (double**)blockB->data;
	double** C = (double**)blockC->data;

	for (uint64 i = range.begin; i < range.end; i+=range.step) {
		for (uint64 j = 0; j < N; ++j) {
			double sum = 0;
			for (uint64 k =0; k<N; ++k) {
				sum += A[i][k] * B[k][j];
			}
			C[i][j] = sum;
		}
	}

	irt_di_free(blockA);
	irt_di_free(blockB);
	irt_di_free(blockC);
	irt_di_destroy(itemA);
	irt_di_destroy(itemB);
	irt_di_destroy(itemC);

	irt_wi_end(wi);
}

void insieme_wi_mul_implementation2(irt_work_item* wi) {

}

void insieme_wi_mul_datareq(irt_work_item* wi, irt_wi_di_requirement* requirements) {

	irt_work_item_range range = wi->range;
	insieme_wi_mul_params* params = ((insieme_wi_mul_params*)(wi->parameters));

	int i =0;

	// dependency A (just a few rows)
	// dim = 1
	requirements[i].di_id = params->A;
	requirements[i].range = (irt_data_range){range.begin, range.end, range.step};
	i++;
	// dim = 2
	requirements[i].di_id = params->A;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;


	// dependency B (all of B)
	// dim = 1
	requirements[i].di_id = params->B;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;
	// dim = 2
	requirements[i].di_id = params->B;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;

	// dependency C (just a few rows)
	// dim = 1
	requirements[i].di_id = params->C;
	requirements[i].range = (irt_data_range){range.begin, range.end, range.step};
	i++;
	// dim = 2
	requirements[i].di_id = params->C;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;
}

void insieme_wi_init_implementation(irt_work_item* wi) {

	// get parameters
	insieme_wi_mul_params *params = (insieme_wi_mul_params*)wi->parameters;

	irt_work_item_range range = wi->range;
	irt_data_range subrange[] = {{range.begin, range.end, range.step}, {0,N,1}};

	irt_data_item* itemA = irt_di_create_sub(irt_data_item_table_lookup(params->A), subrange);
	irt_data_item* itemB = irt_di_create_sub(irt_data_item_table_lookup(params->B), subrange);

	irt_data_block* blockA = irt_di_aquire(itemA, IRT_DMODE_WRITE_FIRST);
	irt_data_block* blockB = irt_di_aquire(itemB, IRT_DMODE_WRITE_FIRST);

	double** A = (double**)blockA->data;
	double** B = (double**)blockB->data;

	for (uint64 i = range.begin; i < range.end; i+=range.step) {
		for (uint64 j = 0; j < N; ++j) {
			A[i][j] = i*j;
			B[i][j] = (i==j)?1:0;
		}
	}

	irt_di_free(blockA);
	irt_di_free(blockB);
	irt_di_destroy(itemA);
	irt_di_destroy(itemB);

	irt_wi_end(wi);
}

void insieme_wi_init_datareq(irt_work_item* wi, irt_wi_di_requirement* requirements) {

	irt_work_item_range range = wi->range;
	insieme_wi_init_params* params = ((insieme_wi_init_params*)(wi->parameters));

	int i =0;

	// dependency A (just a few rows)
	// dim = 1
	requirements[i].di_id = params->A;
	requirements[i].range = (irt_data_range){range.begin, range.end, range.step};
	i++;
	// dim = 2
	requirements[i].di_id = params->A;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;


	// dependency B (all of B)
	// dim = 1
	requirements[i].di_id = params->B;
	requirements[i].range = (irt_data_range){range.begin, range.end, range.step};
	i++;
	// dim = 2
	requirements[i].di_id = params->B;
	requirements[i].range = (irt_data_range){0,N,1};
	i++;

}


