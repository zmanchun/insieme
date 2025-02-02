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

#define _GLIBCXX_USE_NANOSLEEP

#include <gtest/gtest.h>
#include <future>
#include <chrono>

#define IRT_ENABLE_APP_TIME_ACCOUNTING

#define IRT_LIBRARY_MAIN
#define IRT_LIBRARY_NO_MAIN_FUN
#include "irt_library.hxx"

#define N 9000

TEST(AppTimeAccounting, Simple) {
	irt::init(4);
	double init_t = irt_time_rts_get_total();
	irt::run([]() {
		irt::merge(irt::parallel([] {
			double x = 0.0, last_app_t = 0.0;
			for(int i = 0; i < N; i++) {
				for(int j = 0; j < N; j++) {
					x += 0.1;
				}
				irt::master([&] {
					// check that time always increases when wis run
					double app_t = irt_time_wis_get_total();
					EXPECT_GT(app_t, last_app_t);
				});
			}
			irt::master([x] { std::cout << "x: " << x << "\n"; });
		}));
	});
	double wi_t = irt_time_wis_get_total();
	double rs_t = irt_time_rts_get_total() - init_t;
	std::cout << "init_t: " << std::fixed << std::setw(11) << std::setprecision(2) << init_t << "\n";
	std::cout << "wi_t:   " << std::fixed << std::setw(11) << std::setprecision(2) << wi_t << "\n";
	std::cout << "rs_t:   " << std::fixed << std::setw(11) << std::setprecision(2) << rs_t << "\n";
	// check that time doesn't increase when non-wis run in process
	EXPECT_EQ(irt_time_wis_get_total(), wi_t);
	// check that wi time is not larger than total process time
	EXPECT_LE(wi_t, rs_t);
	irt::shutdown();
}

TEST(AppTimeAccounting, AppProgress) {
	bool run = true;
	uint64 lastProgress = 0;
	auto extThread = std::async(std::launch::async, [&]() {
		while(run) {
			uint64 curProgress = irt_app_progress_get();
			EXPECT_LE(lastProgress, curProgress);
			std::cout << "app progress: " << curProgress << "\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});

	irt::init(4);
	irt::run([]() {
		irt::merge(irt::parallel([] {
			double x = 0.0, last_app_t = 0.0;
			for(int i = 0; i < N; i++) {
				for(int j = 0; j < N; j++) {
					x += 0.1;
				}
				irt::barrier();
			}
			irt::master([x] { std::cout << "x: " << x << "\n"; });
		}));
	});
	irt::shutdown();

	run = false;
	extThread.wait();
}
