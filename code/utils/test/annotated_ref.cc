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

#include <string>

#include <gtest/gtest.h>
#include "annotated_ptr.h"

using std::string;

// ------------- utility classes required for the test case --------------

class A {
	void f() {};
};
class B : public A { };


// testing basic properties
TEST(AnnotatedPtr, Basic) {

	EXPECT_LE ( sizeof(AnnotatedPtr<int>) , 2*sizeof(int*) );

	int a = 10;
	int b = 15;

	// test simple creation
	AnnotatedPtr<int> refA(&a);
	EXPECT_EQ (*refA, a);

	// ... and for another element
	AnnotatedPtr<int> refB(&b);
	EXPECT_EQ (*refB, b);

	// test whether modifications are reflected
	a++;
	EXPECT_EQ (*refA, a);

}

TEST(AnnotatedPtrerence, UpCast) {

	// create two related instances
	A a;
	B b;

	// create references
	AnnotatedPtr<A> refA(&a);
	AnnotatedPtr<B> refB(&b);

	// make assignment (if it compiles, test passed!)
	refA = refB;
}


