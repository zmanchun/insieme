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

/**
 * ------------------------ Auto-generated Code ------------------------ 
 *           This code was generated by the Insieme Compiler 
 * --------------------------------------------------------------------- 
 */

#ifdef _MSC_VER
#include <Windows.h> // WINFIX
#endif
#include <ir_interface.h>
#include <irt_all_impls.h>
#include <standalone.h>
//#include <stdint.h> WINFIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WINFIX: math.h must be included (otherwise the according library won't be linked as it seems)
#ifdef _MSC_VER
#include <math.h>
#endif

irt_wi_implementation g_insieme_impl_table[];

// WINFIX: we need to typedef the structs so we have a common macro for this structs and structs like irt_parallel_job
// otherwise we would need to treat them differently
typedef struct __insieme_gen_type_30 {
    irt_type_id c0;
    int32_t c1;
    char** c2;
} _insieme_gen_type_30;

typedef struct __insieme_gen_type_27 {
    irt_type_id c0;
    int32_t* c1;
    float** c2;
    float** c3;
    double* c4;
    float** c5;
} _insieme_gen_type_27;

// --- componenents for type table entries ---
irt_type_id g_type_2_components[] = {1};
irt_type_id g_type_4_components[] = {3};
irt_type_id g_type_5_components[] = {4};
irt_type_id g_type_7_components[] = {6};
irt_type_id g_type_8_components[] = {0,2,5,5,7,5};
irt_type_id g_type_10_components[] = {9};
irt_type_id g_type_11_components[] = {10};
irt_type_id g_type_12_components[] = {0,1,11};

// --- the type table ---
irt_type g_insieme_type_table[] = {
    {IRT_T_UINT32, sizeof(irt_type_id), 0, 0},
    {IRT_T_INT32, sizeof(int32_t), 0, 0},
    {IRT_T_POINTER, sizeof(int32_t*), 1, g_type_2_components},
    {IRT_T_UINT32, sizeof(float), 0, 0},
    {IRT_T_POINTER, sizeof(float*), 1, g_type_4_components},
    {IRT_T_POINTER, sizeof(float**), 1, g_type_5_components},
    {IRT_T_UINT32, sizeof(double), 0, 0},
    {IRT_T_POINTER, sizeof(double*), 1, g_type_7_components},
    {IRT_T_STRUCT, sizeof(_insieme_gen_type_27), 6, g_type_8_components},
    {IRT_T_UINT32, sizeof(char), 0, 0},
    {IRT_T_POINTER, sizeof(char*), 1, g_type_10_components},
    {IRT_T_POINTER, sizeof(char**), 1, g_type_11_components},
    {IRT_T_STRUCT, sizeof(_insieme_gen_type_30), 3, g_type_12_components}
};

double sqrt(double p1);

// WINFIX: type changed from int64_t to clock_t (caused compile error with VS), or just don't write prototype
//clock_t clock(); 

double pow(double p1, double p2);

// WINFIX: void* memset(void* p1, int32_t p2, uint64_t p3);
//void* memset(void* p1, int p2, size_t p3); // either this or don't write prototype

double sin(double p1);


/* --- WINFIX BEGIN helper functions to create structs ------- */

#ifdef _MSC_VER
#define IRT_STACK_STRUCT(__name, ...) (__name##_create(__VA_ARGS__))
#else
#define IRT_STACK_STRUCT(__name, ...) ((__name){__VA_ARGS__})
#endif

inline _insieme_gen_type_27 _insieme_gen_type_27_create(irt_type_id c0, int32_t* c1, float** c2, float** c3,    double* c4,   float** c5) {
	_insieme_gen_type_27 ret = { c0, c1, c2, c3, c4, c5};
	return ret;
}

inline irt_work_item_range irt_work_item_range_create(int64 begin, int64 end, int64 step){
	irt_work_item_range ret = {begin, end, step};
	return ret;
}

inline _insieme_gen_type_30 _insieme_gen_type_30_create(irt_type_id c0, int32_t c1, char** c2){
	_insieme_gen_type_30 ret = {c0, c1, c2};
	return ret;
}

/** creates a irt_parallel_job struct and returns it */
inline irt_parallel_job irt_parallel_job_create(uint32 min, uint32 max, uint32 mod, irt_wi_implementation_id impl_id, irt_lw_data_item* args){
	irt_parallel_job par_job = {min, max, mod, &g_insieme_impl_table[impl_id], args};
	return par_job;
}

/* --- WINFIX END helper functions to create structs ------- */


/* ------- Function Definitions --------- */
double init_func(int32_t x, int32_t y) {
    return (double)40*sin((double)(16*(2*x-1)*y));
}
/* ------- Function Definitions --------- */
int32_t __insieme_fun_8(int32_t argc, char** argv) {
    int64_t start_t = 0;
    int64_t end_t = 0;
    double setup_time = 0.0;
    double elapsed_time = 0.0;
    start_t = clock();
    int32_t N = 650;
    if (argc > 1) {
        N = atoi(argv[1]);
    };
    int32_t numIter = 100;
    if (argc > 2) {
        numIter = atoi(argv[1]);
    };
    float* u;
    float* tmp;
    float* f;
    float* res;
    u = (float*)malloc(sizeof(float)*((uint64_t)(N*N)*sizeof(float)/sizeof(float)));	// WINFIX: missing cast to float* added
    tmp = (float*)malloc(sizeof(float)*((uint64_t)(N*N)*sizeof(float)/sizeof(float)));	// WINFIX: missing cast to float* added
    f = (float*)malloc(sizeof(float)*((uint64_t)(N*N)*sizeof(float)/sizeof(float)));	// WINFIX: missing cast to float* added
    res = (float*)malloc(sizeof(float)*((uint64_t)(N*N)*sizeof(float)/sizeof(float)));	// WINFIX: missing cast to float* added
    if (!(!(u == 0) && !(tmp == 0) && !(f == 0) && !(res == 0))) {
        printf("Error allocating arrays\n");
    };
    memset((void*)u, 0, (uint64_t)(N*N)*sizeof(float));
    memset((void*)f, 0, (uint64_t)(N*N)*sizeof(float));
    for (int32_t var_20 = 0, var_87 = N, var_88 = 1; var_20 < var_87; var_20+=var_88) {
        for (int32_t var_22 = 0, var_89 = N, var_90 = 1; var_22 < var_89; var_22+=var_90) {
            f[var_20*N+var_22] = (float)init_func(var_20, var_22);
        };
    };
    double comm_time = (double)0;
    double comp_time = (double)0;
    double timer = (double)0;
    double resv = 0.0;
    double factor = pow((double)1/(double)N, (double)2);
    end_t = clock();
    start_t = clock();
    for (int32_t var_32 = 0, var_92 = numIter, var_93 = 1; var_32 < var_92; var_32+=var_93) {

		// WINFIX:
        // old: irt_merge(irt_parallel(&(irt_parallel_job){1, 4294967295, 1, 1, (irt_lw_data_item*)(&(struct __insieme_gen_type_27){8, &N, &u, &tmp, &factor, &f})}));
		// new:
		irt_merge(irt_parallel(&(IRT_STACK_STRUCT(irt_parallel_job, 1, 4294967295, 1, &g_insieme_impl_table[1], 
			(irt_lw_data_item*)(&IRT_STACK_STRUCT(_insieme_gen_type_27, 8, &N, &u, &tmp, &factor, &f))))));
        memcpy((void*)u, (void*)tmp, (uint64_t)(N*N)*sizeof(float));
        for (int32_t var_40 = 1, var_101 = N-1, var_102 = 1; var_40 < var_101; var_40+=var_102) {
            for (int32_t var_42 = 1, var_103 = N-1, var_104 = 1; var_42 < var_103; var_42+=var_104) {
                res[var_40*N+var_42] = f[var_40*N+var_42]-(float)4*u[var_40*N+var_42]+u[(var_40-1)*N+var_42]+u[(var_40+1)*N+var_42]+u[var_40*N+var_42-1]+u[var_40*N+var_42+1];
            };
        };
        double norm = (double)0;
        for (int32_t var_45 = 1, var_105 = N-1, var_106 = 1; var_45 < var_105; var_45+=var_106) {
            for (int32_t var_47 = 1, var_107 = N-1, var_108 = 1; var_47 < var_107; var_47+=var_108) {
                norm = norm+pow((double)res[var_45*N+var_47], (double)2);
            };
        };
        resv = sqrt(norm)/(double)(N-1);
    };
    end_t = clock();
    free(u);
    free(tmp);
    free(f);
    free(res);
    printf("Job Done! - residuo: %lf\n", resv);

	irt_exit(0);

	// WINFIX: missing return
	return 0;
}
/* ------- Function Definitions --------- */
void insieme_wi_2_var_0_impl(irt_work_item* var_81) {
    __insieme_fun_8((*(_insieme_gen_type_30*)(*var_81).parameters).c1, (*(_insieme_gen_type_30*)(*var_81).parameters).c2);
}
/* ------- Function Definitions --------- */
void __insieme_fun_23(int32_t* var_65, float** var_66, float** var_67, double* var_68, float** var_69) {
    {
        {
			// WINFIX: old: irt_pfor(irt_wi_get_current(), irt_wi_get_wg(irt_wi_get_current(), 0), (irt_work_item_range){1, *var_65-1, 1}, 0, (irt_lw_data_item*)(&(struct __insieme_gen_type_27){8, var_65, var_66, var_67, var_68, var_69}));
			// new:
            irt_pfor(irt_wi_get_current(), irt_wi_get_wg(irt_wi_get_current(), 0), IRT_STACK_STRUCT(irt_work_item_range, 1, *var_65-1, 1), &g_insieme_impl_table[0], (irt_lw_data_item*)(&IRT_STACK_STRUCT(_insieme_gen_type_27, 8, var_65, var_66, var_67, var_68, var_69)));
            irt_wg_barrier(irt_wi_get_wg(irt_wi_get_current(), 0));
        };
    };
}
/* ------- Function Definitions --------- */
void insieme_wi_1_var_0_impl(irt_work_item* var_77) {
    __insieme_fun_23((*(_insieme_gen_type_27*)(*var_77).parameters).c1, (*(_insieme_gen_type_27*)(*var_77).parameters).c2, (*(_insieme_gen_type_27*)(*var_77).parameters).c3, (*(_insieme_gen_type_27*)(*var_77).parameters).c4, (*(_insieme_gen_type_27*)(*var_77).parameters).c5);
}
/* ------- Function Definitions --------- */
void insieme_wi_0_var_0_impl(irt_work_item* var_71) {
    irt_work_item_range var_72 = (*var_71).range;
    int32_t var_73 = var_72.begin;
    int32_t var_74 = var_72.end;
    int32_t var_75 = var_72.step;
    {
        for (int32_t var_34 = var_73, var_97 = var_74, var_98 = var_75; var_34 < var_97; var_34+=var_98) {
            for (int32_t var_36 = 1, var_99 = *(*(_insieme_gen_type_27*)(*var_71).parameters).c1-1, var_100 = 1; var_36 < var_99; var_36+=var_100) {
                (*(*(_insieme_gen_type_27*)(*var_71).parameters).c3)[var_34**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36] = (float)((double)1/(double)4*((double)((*(*(_insieme_gen_type_27*)(*var_71).parameters).c2)[(var_34-1)**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36]+(*(*(_insieme_gen_type_27*)(*var_71).parameters).c2)[var_34**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36+1]+(*(*(_insieme_gen_type_27*)(*var_71).parameters).c2)[var_34**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36-1]+(*(*(_insieme_gen_type_27*)(*var_71).parameters).c2)[(var_34+1)**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36])-*(*(_insieme_gen_type_27*)(*var_71).parameters).c4*(double)(*(*(_insieme_gen_type_27*)(*var_71).parameters).c5)[var_34**(*(_insieme_gen_type_27*)(*var_71).parameters).c1+var_36]));
            };
        };
    };
}


// --- work item variants ---
irt_wi_implementation_variant g_insieme_wi_0_variants[] = {
    { &insieme_wi_0_var_0_impl, 0, NULL, 0, NULL, 0, NULL },
};
irt_wi_implementation_variant g_insieme_wi_1_variants[] = {
    { &insieme_wi_1_var_0_impl, 0, NULL, 0, NULL, 0, NULL },
};
irt_wi_implementation_variant g_insieme_wi_2_variants[] = {
    { &insieme_wi_2_var_0_impl, 0, NULL, 0, NULL, 0, NULL },
};
// --- the implementation table --- 
irt_wi_implementation g_insieme_impl_table[] = {
    { 1, 1, g_insieme_wi_0_variants },
    { 2, 1, g_insieme_wi_1_variants },
    { 3, 1, g_insieme_wi_2_variants },
};

void insieme_init_context(irt_context* context) {
    context->type_table_size = 13;
    context->type_table = g_insieme_type_table;
    context->impl_table_size = 3;
    context->impl_table = g_insieme_impl_table;
}

void insieme_cleanup_context(irt_context* context) {
}

/* ------- Function Definitions --------- */
int32_t main(int32_t var_79, char** var_80) {
    // WINFIX, old:  irt_runtime_standalone(irt_get_default_worker_count(), &insieme_init_context, &insieme_cleanup_context, 2, (irt_lw_data_item*)(&(struct __insieme_gen_type_30){12, var_79, var_80}));
	// new:
	irt_runtime_standalone(irt_get_default_worker_count(), &insieme_init_context, &insieme_cleanup_context, &g_insieme_impl_table[2], (irt_lw_data_item*)(&IRT_STACK_STRUCT(_insieme_gen_type_30, 12, var_79, var_80)));
    return 0;
}


